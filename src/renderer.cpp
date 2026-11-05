#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include "glad.h"
#endif
#include "renderer.hpp"
#include "webcam.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace prism {
namespace {
constexpr int bindingSamples=12;
std::string read(const std::string& path) {
    std::ifstream file(path); if(!file)throw std::runtime_error("Missing shader: "+path);
    return {std::istreambuf_iterator<char>(file),{}};
}
GLuint shader(GLenum type,const std::string& source) {
    GLuint id=glCreateShader(type);const char* p=source.c_str();glShaderSource(id,1,&p,nullptr);glCompileShader(id);
    GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok) {char log[8192];glGetShaderInfoLog(id,sizeof log,nullptr,log);glDeleteShader(id);throw std::runtime_error(log);}
    return id;
}
GLuint program(const std::string& vert,const std::string& frag,bool feedback=false,bool instanced=false) {
    GLuint v=shader(GL_VERTEX_SHADER,vert),f=shader(GL_FRAGMENT_SHADER,frag),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);
    if(feedback) {
        glBindAttribLocation(p,0,"a_position");glBindAttribLocation(p,1,"a_velocity");
        glBindAttribLocation(p,2,"a_meta");glBindAttribLocation(p,3,"a_seed");
        const char* varyings[]={"v_position","v_velocity","v_meta"};
        glTransformFeedbackVaryings(p,3,varyings,GL_SEPARATE_ATTRIBS);
    } else if(instanced) {
        const char* names[]={"position","normal","i_position","i_velocity","i_meta","i_seed"};
        for(int i=0;i<6;++i) glBindAttribLocation(p,i,names[i]);
    }
    glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof log,nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}
    return p;
}
// Names are string literals at each call site; views remain valid for the
// program lifetime. Resolve each uniform once, outside the steady-state loop.
std::unordered_map<GLuint,std::unordered_map<std::string_view,GLint>> locations;
GLint location(GLuint p,const char* name) {
    auto& cache=locations[p];auto found=cache.find(name);
    if(found!=cache.end())return found->second;
    return cache.emplace(name,glGetUniformLocation(p,name)).first->second;
}
void uniform(GLuint p,const char* name,float value){glUniform1f(location(p,name),value);}
void uniform3(GLuint p,const char* name,const float* value){glUniform3fv(location(p,name),1,value);}
void audioUniforms(GLuint p,const Features& f,float response) {
    glUniform4f(location(p,"u_audio"),f.bass,f.mid,f.high,f.beat);
    uniform(p,"u_energy",f.energy);
    uniform(p,"u_reactivity",response);
}
void attribute(int index,GLuint buffer,int size=4,int divisor=0,int stride=0,size_t offset=0) {
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glEnableVertexAttribArray(index);
    glVertexAttribPointer(index,size,GL_FLOAT,GL_FALSE,stride,reinterpret_cast<void*>(offset));
    glVertexAttribDivisor(index,divisor);
}
GLuint texture(int width,int height,GLint format) {
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return t;
}
void sampler(GLuint program,const char* name,int unit,GLuint tex) {
    glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,tex);glUniform1i(location(program,name),unit);
}
void bufferSampler(GLuint program,const char* name,int unit,GLuint tex){
    glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_BUFFER,tex);glUniform1i(location(program,name),unit);
}
void flowUniforms(GLuint program,const Look& p,const Features& f,float response,float time,float dt,int slot,int width,int height){
    uniform(program,"u_time",time);uniform(program,"u_dt",dt);uniform(program,"u_routes",28);uniform(program,"u_slot",float(slot));
    #define FLOW(name) uniform(program,"u_" #name,p.name)
    FLOW(flowScale);FLOW(flowSpeed);FLOW(evolve);FLOW(drift);FLOW(steer);FLOW(spawnRadius);FLOW(shapeSize);FLOW(cling);
    #undef FLOW
    uniform(program,"u_shape",float(p.shape));uniform(program,"u_nextShape",float(p.nextShape));uniform(program,"u_shapeBlend",p.shapeBlend);
    glUniform2f(location(program,"u_life"),p.lifeMin,std::max(p.lifeMin+.5f,p.lifeMax));
    const float aspect=height>0?float(width)/height:1.6f;
    glUniform3f(location(program,"u_bounds"),std::max(14.f,14*aspect+2),13,p.shapeSize*2/3);
    audioUniforms(program,f,response);
}
bool verifyGradient(const std::string& source,std::string& report) {
    // Test the actual GPU derivative against central differences of the
    // preserved scalar noise, independent of the particle motion checks.
    const auto prefix=source.substr(0,source.find("void main()"));
    const std::string test=prefix+R"GLSL(
void main() {
  vec3 p=a_position.xyz;
  const float e=.002;
  vec3 numeric=vec3(
    snoise(p+vec3(e,0,0))-snoise(p-vec3(e,0,0)),
    snoise(p+vec3(0,e,0))-snoise(p-vec3(0,e,0)),
    snoise(p+vec3(0,0,e))-snoise(p-vec3(0,0,e)))/(2.*e);
  v_position=vec4(noiseGradient(p),1.);
  v_velocity=vec4(numeric,1.);
  v_meta=vec4(0.);
}
)GLSL";
    GLuint p=program(test,"#version 330 core\nout vec4 c;void main(){c=vec4(0.);}",true);
    constexpr int count=512;
    std::vector<float> input(count*4),analytic(count*4),numeric(count*4);Random random(394837);
    for(auto& v:input)v=(random.next()-.5f)*100;
    GLuint vao=0,buffers[4]{};glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(4,buffers);
    glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferData(GL_ARRAY_BUFFER,input.size()*sizeof(float),input.data(),GL_STATIC_DRAW);
    attribute(0,buffers[0]);
    for(int k=1;k<4;++k){glBindBuffer(GL_ARRAY_BUFFER,buffers[k]);glBufferData(GL_ARRAY_BUFFER,input.size()*sizeof(float),nullptr,GL_DYNAMIC_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,k-1,buffers[k]);}
    glUseProgram(p);glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,count);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
    glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glGetBufferSubData(GL_ARRAY_BUFFER,0,analytic.size()*sizeof(float),analytic.data());
    glBindBuffer(GL_ARRAY_BUFFER,buffers[2]);glGetBufferSubData(GL_ARRAY_BUFFER,0,numeric.size()*sizeof(float),numeric.data());
    float worst=0,mean=0;bool finite=true;
    for(int i=0;i<count;++i){float error=0,magnitude=0;
        for(int k=0;k<3;++k){float a=analytic[i*4+k],n=numeric[i*4+k];finite&=std::isfinite(a)&&std::isfinite(n);error+=(a-n)*(a-n);magnitude+=a*a;}
        float relative=std::sqrt(error)/std::max(1.f,std::sqrt(magnitude));worst=std::max(worst,relative);mean+=relative/count;
    }
    for(int k=0;k<3;++k)glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,k,0);
    glBindVertexArray(0);glUseProgram(0);glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(4,buffers);glDeleteVertexArrays(1,&vao);glDeleteProgram(p);
    report="; gradient mean="+std::to_string(mean)+", max="+std::to_string(worst);
    return finite&&mean<.005f&&worst<.08f;
}
bool verifyFeedback(GLuint effect,GLuint triangle,const std::string& full,const std::string& prefix,std::string& report) {
    // Exercise the actual GPU recurrence with a constant pulse followed by an
    // exact sky. This detects time/retention errors, self-sampling and runaway
    // accumulation independently of the evolving particle image.
    constexpr int size=4;
    GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
    GLuint images[5]{},targets[2]{};
    for(auto& id:images)id=texture(size,size,GL_RGBA32F);
    glGenFramebuffers(2,targets);glBindVertexArray(triangle);
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glViewport(0,0,size,size);
    std::array<float,size*size*4> pulse{};
    for(int i=0;i<size*size;++i){pulse[i*4]=.6f;pulse[i*4+1]=.01f;pulse[i*4+2]=.02f;pulse[i*4+3]=1;}
    glBindTexture(GL_TEXTURE_2D,images[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,size,size,GL_RGBA,GL_FLOAT,pulse.data());
    const GLuint sky=program(full,prefix+"\nvoid main(){vec2 p=gl_FragCoord.xy;colour=vec4(finishColour(sky(p),p),1.);}");
    auto skyUniforms=[&](GLuint p){
        glUseProgram(p);glUniform2f(location(p,"u_resolution"),size,size);uniform(p,"u_time",0);uniform(p,"u_dark",1);
        const float zero[4]{};for(const auto* name:{"u_bgA","u_bgB","u_bgC","u_bgD"})uniform3(p,name,zero);
        glUniform4fv(location(p,"u_bgP"),1,zero);
    };
    glBindFramebuffer(GL_FRAMEBUFFER,targets[0]);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[1],0);
    skyUniforms(sky);glDrawArrays(GL_TRIANGLES,0,3);
    const GLenum attachments[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
    bool complete=true;
    for(int i=0;i<2;++i){
        glBindFramebuffer(GL_FRAMEBUFFER,targets[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,images[2],0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,images[3+i],0);
        glDrawBuffers(2,attachments);complete&=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    }
    skyUniforms(effect);uniform(effect,"u_persistence",1.25f);uniform(effect,"u_feedbackDrift",0);uniform(effect,"u_feedbackHit",1);
    glUniform2f(location(effect,"u_feedbackCentre"),.5f,.5f);
    int current=0;
    auto pass=[&](bool deposit,float dt,bool valid){
        const int next=1-current;glBindFramebuffer(GL_FRAMEBUFFER,targets[next]);
        sampler(effect,"b_composite",0,images[deposit?0:1]);sampler(effect,"b_history",1,images[3+current]);
        uniform(effect,"u_feedbackDt",dt);glUniform1i(location(effect,"u_historyValid"),valid?1:0);
        glDrawArrays(GL_TRIANGLES,0,3);current=next;
    };
    auto value=[&](){
        std::array<float,4> v{};glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(2,2,1,1,GL_RGBA,GL_FLOAT,v.data());return v;
    };
    pass(true,1/60.f,false);const auto initial=value();
    for(int i=0;i<60;++i)pass(false,1/60.f,true);const auto at60=value();
    pass(true,1/60.f,false);
    for(int i=0;i<240;++i)pass(false,1/240.f,true);const auto at240=value();
    pass(false,0,true);const auto frozen=value();
    pass(true,1/60.f,false);
    for(int i=0;i<120;++i)pass(true,1/60.f,true);const auto sustained=value();
    pass(false,0,false);const auto cleared=value();
    float worst=0;bool finite=complete;
    for(int k=0;k<4;++k){
        for(const auto& v:{initial,at60,at240,frozen,sustained,cleared})finite&=std::isfinite(v[k]);
        const float expected=initial[k]*std::exp(-1/1.25f);
        worst=std::max({worst,std::abs(at60[k]-expected),std::abs(at240[k]-expected),std::abs(frozen[k]-at240[k]),std::abs(sustained[k]-initial[k]),std::abs(cleared[k])});
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);glBindVertexArray(0);glUseProgram(0);
    glDeleteFramebuffers(2,targets);glDeleteTextures(5,images);locations.erase(sky);glDeleteProgram(sky);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
    report="; feedback recurrence error="+std::to_string(worst);
    return finite&&initial[0]>.1f&&worst<.00005f;
}
}
struct Renderer::State {
    GLuint sim=0,prism=0,post=0,dof=0,feedback=0;
    GLuint simVao[2]{},drawVao[2]{},buffer[2][3]{},seed=0,box=0,triangle=0;
    GLuint scene=0,composite=0,colour=0,normals=0,depth=0,compositeTex=0,paletteTex=0;
    GLuint lensTarget=0,lensTexture=0;
    Texture2D webcam{};
    std::vector<uint8_t> webcamCrimson;
    float webcamMean=0,webcamContrastScale=1;
    GLuint feedbackTargets[2]{},historyTextures[2]{},feedbackTexture=0;
    int historyIndex=0;
    bool historyValid=false,historyPatchView=false,historyDark=true;
    GLuint bindingAnchors=0,bindingGrow=0,anchorVao=0,bindingVao=0,bindingDrawVao=0,bindingSeed=0;
    GLuint anchorBuffers[3]{},anchorTextures[3]{},bindingBuffers[3]{};
    int bindingCount=0;
    bool bindingsActive=false;
    float rootProjectionError=0;
    std::array<float,28*2*16> paletteData{};
    std::array<PaletteRow,2> backgrounds{};
    std::array<float,16> bg{};
    int current=0,width=0,height=0,slot=0,activeCount=maxParticles;
    float time=0,dark=1;
    bool crimson=true;
    std::string simulationSource,postPrefix,fullTriangle;
    void deleteTargets() {
        glDeleteFramebuffers(1,&scene);glDeleteFramebuffers(1,&composite);
        glDeleteTextures(1,&colour);glDeleteTextures(1,&normals);glDeleteTextures(1,&compositeTex);glDeleteRenderbuffers(1,&depth);
        glDeleteFramebuffers(1,&lensTarget);glDeleteTextures(1,&lensTexture);
        glDeleteFramebuffers(2,feedbackTargets);glDeleteTextures(2,historyTextures);glDeleteTextures(1,&feedbackTexture);
        std::fill_n(feedbackTargets,2,0);std::fill_n(historyTextures,2,0);feedbackTexture=0;historyValid=false;historyIndex=0;
        scene=composite=colour=normals=compositeTex=depth=lensTarget=lensTexture=0;
    }
    void targets(int w,int h) {
        if(width==w&&height==h)return;
        deleteTargets();width=w;height=h;
        colour=texture(w,h,GL_RGBA32F);normals=texture(w,h,GL_RGBA16F);compositeTex=texture(w,h,GL_RGBA16F);
        glGenFramebuffers(1,&scene);glBindFramebuffer(GL_FRAMEBUFFER,scene);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,normals,0);
        glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        const GLenum attachments[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,attachments);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU scene framebuffer incomplete");
        glGenFramebuffers(1,&composite);glBindFramebuffer(GL_FRAMEBUFFER,composite);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,compositeTex,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU composite framebuffer incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    void feedbackBuffers() {
        if(feedbackTexture)return;
        feedbackTexture=texture(width,height,GL_RGBA16F);
        glGenFramebuffers(2,feedbackTargets);
        const GLenum attachments[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
        for(int i=0;i<2;++i){
            // Float history avoids quantisation building up over many quiet frames.
            historyTextures[i]=texture(width,height,GL_RGBA32F);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER,feedbackTargets[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,feedbackTexture,0);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,historyTextures[i],0);
            glDrawBuffers(2,attachments);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU feedback framebuffer incomplete");
        }
    }
};
Renderer::Renderer(const std::string& assets):state(std::make_unique<State>()) {
    auto& s=*state;
    const std::string full="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}";
    s.fullTriangle=full;
    s.simulationSource=read(assets+"/shaders/simulate.vert");
    s.sim=program(s.simulationSource,"#version 330 core\nout vec4 c;void main(){c=vec4(0.);}",true);
    const auto flow=s.simulationSource.substr(0,s.simulationSource.find("void main()"))+"\n#define BINDING_SAMPLES "+std::to_string(bindingSamples)+"\n";
    const std::string emptyFragment="#version 330 core\nout vec4 c;void main(){c=vec4(0.);}";
    const auto bodyMotion=read(assets+"/shaders/body-motion.glsl");
    s.bindingAnchors=program(flow+bodyMotion+"\n"+read(assets+"/shaders/bindings-anchors.vert"),emptyFragment,true);
    s.bindingGrow=program(flow+read(assets+"/shaders/bindings-grow.vert"),emptyFragment,true);
    auto prismSource=read(assets+"/shaders/prism.vert");
    prismSource.insert(prismSource.find('\n')+1,bodyMotion+"\n");
    s.prism=program(prismSource,read(assets+"/shaders/prism.frag"),false,true);
    const auto postSource=read(assets+"/shaders/post.frag");
    s.postPrefix=postSource.substr(0,postSource.find("vec3 shade("));
    s.post=program(full,postSource);s.dof=program(full,read(assets+"/shaders/dof.frag"));
    s.feedback=program(full,s.postPrefix+read(assets+"/shaders/feedback.frag"));
    glGenVertexArrays(1,&s.triangle);
    // Six individually shaded cube faces: local x follows the current.
    constexpr float corners[8][3]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
    constexpr int faces[6][4]={{1,0,3,2},{4,5,6,7},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
    constexpr float normals[6][3]={{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    constexpr int triangles[6]={0,1,2,0,2,3};
    std::vector<float> cube;
    for(int f=0;f<6;++f)for(int t:triangles){for(float v:corners[faces[f][t]])cube.push_back(v);for(float v:normals[f])cube.push_back(v);}
    glGenBuffers(1,&s.box);glBindBuffer(GL_ARRAY_BUFFER,s.box);glBufferData(GL_ARRAY_BUFFER,cube.size()*sizeof(float),cube.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&s.seed);std::vector<float> seeds(maxParticles*4);Random r(432548);
    for(auto& v:seeds)v=r.next();
    glBindBuffer(GL_ARRAY_BUFFER,s.seed);glBufferData(GL_ARRAY_BUFFER,seeds.size()*sizeof(float),seeds.data(),GL_STATIC_DRAW);
    glGenVertexArrays(2,s.simVao);glGenVertexArrays(2,s.drawVao);
    for(int side=0;side<2;++side) {
        glGenBuffers(3,s.buffer[side]);
        for(int k=0;k<3;++k){glBindBuffer(GL_ARRAY_BUFFER,s.buffer[side][k]);glBufferData(GL_ARRAY_BUFFER,maxParticles*4*sizeof(float),nullptr,GL_DYNAMIC_COPY);}
        glBindVertexArray(s.simVao[side]);for(int k=0;k<3;++k)attribute(k,s.buffer[side][k]);attribute(3,s.seed);
        glBindVertexArray(s.drawVao[side]);attribute(0,s.box,3,0,6*sizeof(float));attribute(1,s.box,3,0,6*sizeof(float),3*sizeof(float));
        for(int k=0;k<3;++k)attribute(k+2,s.buffer[side][k],4,1);attribute(5,s.seed,4,1);
    }
    s.paletteTex=texture(4,56,GL_RGBA32F);
    // Two small branching feeds, with three braided fibres. The same primitive
    // and material shader draw both these segments and the simulated ribbons.
    std::vector<float> bindingSeeds;
    for(int root=0;root<2;++root)for(int part=0;part<4;++part){
        const int segments=part==0?72:60,strands=part==0?3:5;
        for(int strand=0;strand<strands;++strand)for(int i=0;i<segments;++i){
            const float values[]={float(root*4+part),float(i)/segments,2.f*strand/(strands-1)-1,1.f/segments};
            bindingSeeds.insert(bindingSeeds.end(),values,values+4);
        }
    }
    s.bindingCount=int(bindingSeeds.size()/4);
    glGenVertexArrays(1,&s.anchorVao);glGenVertexArrays(1,&s.bindingVao);glGenVertexArrays(1,&s.bindingDrawVao);
    glGenBuffers(3,s.anchorBuffers);glGenBuffers(3,s.bindingBuffers);glGenTextures(3,s.anchorTextures);
    for(int k=0;k<3;++k){
        glBindBuffer(GL_ARRAY_BUFFER,s.anchorBuffers[k]);glBufferData(GL_ARRAY_BUFFER,6*bindingSamples*4*sizeof(float),nullptr,GL_DYNAMIC_COPY);
        glBindTexture(GL_TEXTURE_BUFFER,s.anchorTextures[k]);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,s.anchorBuffers[k]);
        glBindBuffer(GL_ARRAY_BUFFER,s.bindingBuffers[k]);glBufferData(GL_ARRAY_BUFFER,s.bindingCount*4*sizeof(float),nullptr,GL_DYNAMIC_COPY);
    }
    glBindTexture(GL_TEXTURE_BUFFER,0);glGenBuffers(1,&s.bindingSeed);
    glBindBuffer(GL_ARRAY_BUFFER,s.bindingSeed);glBufferData(GL_ARRAY_BUFFER,bindingSeeds.size()*sizeof(float),bindingSeeds.data(),GL_STATIC_DRAW);
    glBindVertexArray(s.bindingVao);attribute(3,s.bindingSeed);
    glBindVertexArray(s.bindingDrawVao);attribute(0,s.box,3,0,6*sizeof(float));attribute(1,s.box,3,0,6*sizeof(float),3*sizeof(float));
    for(int k=0;k<3;++k)attribute(k+2,s.bindingBuffers[k],4,1);attribute(5,s.bindingSeed,4,1);
    palette(0,161301,0);palette(13,959004,1);reset();
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glUseProgram(0);
}
Renderer::~Renderer() {
    auto& s=*state;s.deleteTargets();
    clearWebcam();
    for(auto p:{s.sim,s.prism,s.post,s.dof,s.feedback,s.bindingAnchors,s.bindingGrow}){locations.erase(p);glDeleteProgram(p);}
    glDeleteVertexArrays(2,s.simVao);glDeleteVertexArrays(2,s.drawVao);glDeleteVertexArrays(1,&s.triangle);
    glDeleteBuffers(3,s.buffer[0]);glDeleteBuffers(3,s.buffer[1]);glDeleteBuffers(1,&s.seed);glDeleteBuffers(1,&s.box);glDeleteTextures(1,&s.paletteTex);
    glDeleteVertexArrays(1,&s.anchorVao);glDeleteVertexArrays(1,&s.bindingVao);glDeleteVertexArrays(1,&s.bindingDrawVao);
    glDeleteBuffers(3,s.anchorBuffers);glDeleteBuffers(3,s.bindingBuffers);glDeleteBuffers(1,&s.bindingSeed);glDeleteTextures(3,s.anchorTextures);
}
void Renderer::uploadWebcamFrame(const WebcamFrame& frame) {
    if(frame.width<=0 || frame.height<=0 ||
       frame.bgra.size()!=size_t(frame.width)*frame.height*4)return;
    // Grade the camera once per new frame so the surface and Material preview
    // use the same ink, without doing colour conversion for every prism pixel.
    static const auto ramp=[]{
        std::array<std::array<uint8_t,4>,256> result{};
        for(size_t i=0;i<result.size();++i){
            const float luma=std::clamp((float(i)-7.f)/225.f,0.f,1.f);
            const float ink=luma*luma*(3.f-2.f*luma);
            result[i]={uint8_t(2+17*ink),uint8_t(1+5*ink),uint8_t(5+174*ink),255};
        }
        return result;
    }();
    auto& graded=state->webcamCrimson;
    graded.resize(frame.bgra.size());
    uint64_t redSum=0;
    unsigned darkest=255,brightest=0;
    for(size_t i=0;i<graded.size();i+=4){
        const auto* pixel=frame.bgra.data()+i;
        const unsigned luma=(19u*pixel[0]+183u*pixel[1]+54u*pixel[2])>>8;
        std::copy(ramp[luma].begin(),ramp[luma].end(),graded.data()+i);
        const unsigned red=ramp[luma][2];
        redSum+=red;
        darkest=std::min(darkest,red);
        brightest=std::max(brightest,red);
    }
    const float mean=float(double(redSum)/double(graded.size()/4));
    state->webcamMean=mean/255.f;
    // A floor keeps low-light noise from turning into strong texture contrast.
    state->webcamContrastScale=255.f/std::max({48.f,mean-darkest,brightest-mean});
    auto& video=state->webcam;
    glActiveTexture(GL_TEXTURE0);
    if(!video.id){
        glGenTextures(1,&video.id);
        glBindTexture(GL_TEXTURE_2D,video.id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        video.mipmaps=1;
        video.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    }else{
        glBindTexture(GL_TEXTURE_2D,video.id);
    }
    if(video.width!=frame.width || video.height!=frame.height){
        video.width=frame.width;
        video.height=frame.height;
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,frame.width,frame.height,0,GL_BGRA,GL_UNSIGNED_BYTE,graded.data());
    }else{
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,frame.width,frame.height,GL_BGRA,GL_UNSIGNED_BYTE,graded.data());
    }
    glBindTexture(GL_TEXTURE_2D,0);
}

void Renderer::clearWebcam() {
    if(!state->webcam.id)return;
    glDeleteTextures(1,&state->webcam.id);
    state->webcam={};
    state->webcamCrimson.clear();
    clearFeedback();
}

Texture2D Renderer::webcamPreview() const {return state->webcam;}

void Renderer::reset() {
    auto& s=*state;s.time=0;s.current=0;s.historyValid=false;
    Random random(674633);std::vector<float> values(maxParticles*4,0);
    for(int k=0;k<3;++k){
        std::fill(values.begin(),values.end(),0.f);
        for(int i=0;i<maxParticles;++i)values[i*4+3]=k==0?-random.next()*14.f:k==1?1.f:0.f;
        for(int side=0;side<2;++side){glBindBuffer(GL_ARRAY_BUFFER,s.buffer[side][k]);glBufferSubData(GL_ARRAY_BUFFER,0,values.size()*sizeof(float),values.data());}
    }
    glBindBuffer(GL_ARRAY_BUFFER,0);
}
void Renderer::clearFeedback(){state->historyValid=false;}
void Renderer::palette(int index,uint32_t seed,int slot) {
    auto& s=*state;auto rows=rollPalette(index,seed);s.backgrounds[slot]=rows[0];
    s.crimson=index==crimsonScheme;
    for(int i=0;i<28;++i){const auto& r=rows[i+1];const Triple coefficients[]={r.coeffs.a,r.coeffs.b,r.coeffs.c,r.coeffs.d};
        const float alpha[]={r.length,r.offset,r.strength,r.ambience};
        for(int c=0;c<4;++c){size_t at=(slot*28+i)*16+c*4;for(int k=0;k<3;++k)s.paletteData[at+k]=coefficients[c][k];s.paletteData[at+3]=alpha[c];}
    }
    glBindTexture(GL_TEXTURE_2D,s.paletteTex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,56,GL_RGBA,GL_FLOAT,s.paletteData.data());
    if(s.bg[15]==0){const auto& r=s.backgrounds[0];const Triple coeffs[]={r.coeffs.a,r.coeffs.b,r.coeffs.c,r.coeffs.d};
        for(int c=0;c<4;++c)for(int k=0;k<3;++k)s.bg[c*3+k]=coeffs[c][k];
        s.bg[12]=r.length;s.bg[13]=r.offset;s.bg[14]=r.strength;s.bg[15]=r.ambience==0?.000001f:r.ambience;
    }
}
void Renderer::useSlot(int slot){state->slot=slot;}
void Renderer::step(float dt,const Look& p,const Features& f) {
    auto& s=*state;s.time+=dt;glUseProgram(s.sim);
    flowUniforms(s.sim,p,f,reactivity,s.time,dt,s.slot,GetScreenWidth(),GetScreenHeight());
    glBindVertexArray(s.simVao[s.current]);
    for(int i=0;i<3;++i)glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,i,s.buffer[1-s.current][i]);
    s.activeCount=std::clamp(p.count,1000,maxParticles);
    glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,s.activeCount);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
    for(int i=0;i<3;++i)glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,i,0);
    glBindVertexArray(0);s.current=1-s.current;glUseProgram(0);
}
void Renderer::draw(const Look& p,const Features& f,int width,int height,const SceneBindings& bindings,float frameDt) {
    auto& s=*state;rlDrawRenderBatchActive();
    // Bring up the quieter mid and treble bands without unbounded movement
    // at the top of the Reactivity range.
    const auto drive=[this](float level,float weight){
        const float input=level*reactivity*weight;
        return input/(1+input);
    };
    const float bass=drive(f.bass,2.f);
    const float mids=drive(f.mid,4.f);
    const float treble=drive(f.high,15.f);
    const float beatScale=1+.035f*drive(f.beat,1.f);
    const auto setBodyMotion=[&](GLuint program){
        const float radial=beatScale*(1+.28f*bass);
        glUniform3f(location(program,"u_bodyScale"),radial,beatScale*(1-.19f*bass),radial);
        glUniform2f(location(program,"u_foldTwist"),1.8f*mids,1/std::max(3.f,p.shapeSize*.8f));
    };
    // Give the living form its own open space beside the patch. The same
    // off-axis projection also places the roots and reconstructs AO positions.
    const Vector2 viewOffset=bindings.visible?Vector2{.47f,-.06f}:Vector2{};
    const float tanFov=std::tan(p.fov*DEG2RAD/2)/(bindings.visible?.60f:1.f);
    const float aspect=float(width)/height;
    const Matrix viewMatrix=MatrixLookAt(camera.position,camera.target,camera.up);
    Matrix projectionMatrix=MatrixPerspective(2*std::atan(tanFov),aspect,.5,400);
    projectionMatrix.m8=-viewOffset.x;projectionMatrix.m9=-viewOffset.y;
    s.bindingsActive=bindings.visible;
    if(bindings.visible){
        const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
        const auto right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
        const auto up=Vector3CrossProduct(right,forward),front=Vector3Negate(forward);
        const float rootDepth=std::max(1.5f,Vector3Distance(camera.position,camera.target)*.5f);
        std::array<Vector3,2> roots{};s.rootProjectionError=0;
        for(int i=0;i<2;++i){
            const float x=2*bindings.roots[i].x/width-1-viewOffset.x;
            const float y=1-2*bindings.roots[i].y/height-viewOffset.y;
            const auto direction=Vector3Add(forward,Vector3Add(Vector3Scale(right,x*aspect*tanFov),Vector3Scale(up,y*tanFov)));
            roots[i]=Vector3Add(camera.position,Vector3Scale(direction,rootDepth));
            const auto v=Vector3Transform(roots[i],viewMatrix);
            const Vector2 screen{(projectionMatrix.m0*v.x/-v.z+viewOffset.x+1)*width*.5f,
                                 (1-projectionMatrix.m5*v.y/-v.z-viewOffset.y)*height*.5f};
            s.rootProjectionError=std::max(s.rootProjectionError,Vector2Distance(screen,bindings.roots[i]));
        }
        glEnable(GL_RASTERIZER_DISCARD);glUseProgram(s.bindingAnchors);
        flowUniforms(s.bindingAnchors,p,f,reactivity,s.time,0,s.slot,width,height);
        setBodyMotion(s.bindingAnchors);
        glBindVertexArray(s.anchorVao);
        for(int k=0;k<3;++k)glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,k,s.anchorBuffers[k]);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,6*bindingSamples);glEndTransformFeedback();
        glUseProgram(s.bindingGrow);uniform(s.bindingGrow,"u_time",s.time);uniform(s.bindingGrow,"u_slot",float(s.slot));
        uniform(s.bindingGrow,"u_bindingSize",p.size/.3f);audioUniforms(s.bindingGrow,f,reactivity);
        uniform(s.bindingGrow,"u_rootPixel",2*rootDepth*tanFov/height);
        glUniform3fv(location(s.bindingGrow,"u_bindingRoots"),2,&roots[0].x);
        uniform3(s.bindingGrow,"u_cameraRight",&right.x);uniform3(s.bindingGrow,"u_cameraUp",&up.x);uniform3(s.bindingGrow,"u_cameraFront",&front.x);
        bufferSampler(s.bindingGrow,"u_anchorPositions",2,s.anchorTextures[0]);bufferSampler(s.bindingGrow,"u_anchorVelocities",3,s.anchorTextures[1]);bufferSampler(s.bindingGrow,"u_anchorMeta",4,s.anchorTextures[2]);
        glBindVertexArray(s.bindingVao);
        for(int k=0;k<3;++k)glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,k,s.bindingBuffers[k]);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,s.bindingCount);glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);
        for(int k=0;k<3;++k){glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,k,0);glActiveTexture(GL_TEXTURE2+k);glBindTexture(GL_TEXTURE_BUFFER,0);}
        glActiveTexture(GL_TEXTURE0);
    }
    // Render at one physical pixel per logical point. Retina UI stays sharp;
    // the expensive geometry/AO passes avoid a redundant 4x pixel workload.
    s.targets(width,height);
    glViewport(0,0,width,height);glBindFramebuffer(GL_FRAMEBUFFER,s.scene);
    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_CULL_FACE);
    glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(s.prism);
    uniform(s.prism,"u_routes",28);
    #define PRISM(name) uniform(s.prism,"u_" #name,p.name)
    uniform(s.prism,"u_crimson",s.crimson?1.f:0.f);
    uniform(s.prism,"u_living",0);
    setBodyMotion(s.prism);
    uniform(s.prism,"u_treble",treble);
    uniform(s.prism,"u_time",s.time);
    PRISM(size);PRISM(stretch);PRISM(spread);PRISM(shimmer);PRISM(paletteScale);PRISM(paletteOffset);PRISM(unify);PRISM(gloss);PRISM(uniformity);PRISM(nearFade);
    #undef PRISM
    audioUniforms(s.prism,f,reactivity);glUniform1fv(location(s.prism,"u_bands"),28,f.bands.data());sampler(s.prism,"u_palette",0,s.paletteTex);
    sampler(s.prism,"u_webcam",1,s.webcam.id?s.webcam.id:s.paletteTex);
    glUniform1i(location(s.prism,"u_webcamEnabled"),s.webcam.id!=0);
    if(s.webcam.id){
        const float aspect=float(s.webcam.width)/s.webcam.height;
        const float scale=.44f/std::max(p.shapeSize,1.f);
        glUniform2f(location(s.prism,"u_webcamScale"),scale*std::min(1.f,1.f/aspect),scale*std::min(1.f,aspect));
        uniform(s.prism,"u_webcamWarpTime",s.time*.43f);
        glUniform2f(location(s.prism,"u_webcamLevels"),s.webcamMean,s.webcamContrastScale);
        const float light=std::max(.18f,1+reactivity*(-.65f+f.energy*.95f+f.beat*.9f));
        const auto swatch=palettePreview();
        float luma=0;
        for(const auto& colour:swatch){
            luma+=.2126f*std::min(255.f,colour.r*light)
                 +.7152f*std::min(255.f,colour.g*light)
                 +.0722f*std::min(255.f,colour.b*light);
        }
        uniform(s.prism,"u_webcamBaseLuma",luma/(255.f*swatch.size()));
        glUniform3f(location(s.prism,"u_webcamOrigin"),viewMatrix.m12,viewMatrix.m13,viewMatrix.m14);
    }
    auto view=MatrixToFloatV(viewMatrix);
    auto projection=MatrixToFloatV(projectionMatrix);
    glUniformMatrix4fv(location(s.prism,"u_viewMatrix"),1,GL_FALSE,view.v);
    glUniformMatrix4fv(location(s.prism,"u_projectionMatrix"),1,GL_FALSE,projection.v);
    glBindVertexArray(s.drawVao[s.current]);glDrawArraysInstanced(GL_TRIANGLES,0,36,std::clamp(p.count,1000,maxParticles));
    if(bindings.visible){
        uniform(s.prism,"u_living",1);glBindVertexArray(s.bindingDrawVao);glDrawArraysInstanced(GL_TRIANGLES,0,36,s.bindingCount);
    }
    glDisable(GL_DEPTH_TEST);glBindFramebuffer(GL_FRAMEBUFFER,s.composite);
    glUseProgram(s.post);glBindVertexArray(s.triangle);
    sampler(s.post,"b_scene",0,s.colour);sampler(s.post,"b_normal",1,s.normals);
    // Shade each logical pixel once, then upscale only the finished image.
    // Running this pass directly on Retina output repeats AO four times.
    glUniform2f(location(s.post,"u_resolution"),float(width),float(height));
    uniform(s.post,"u_pixelScale",1.f);
    uniform(s.post,"u_time",s.time);uniform(s.post,"u_tanFov",tanFov);uniform(s.post,"u_dpr",1);
    glUniform2f(location(s.post,"u_viewOffset"),viewOffset.x,viewOffset.y);
    #define POST(name) uniform(s.post,"u_" #name,p.name)
    POST(lines);POST(ao);POST(aoRadius);POST(fog);
    #undef POST
    uniform(s.post,"u_fogStart",std::max(0.f,Vector3Distance(camera.position,camera.target)-10));
    s.dark+=(float(p.dark)-s.dark)*.08f;uniform(s.post,"u_dark",s.dark);
    const auto& r=s.backgrounds[s.slot];const Triple coeffs[]={r.coeffs.a,r.coeffs.b,r.coeffs.c,r.coeffs.d};
    const float props[]={r.length,r.offset,r.strength,r.ambience};
    for(int c=0;c<4;++c)for(int k=0;k<3;++k)s.bg[c*3+k]+=(coeffs[c][k]-s.bg[c*3+k])*.018f;
    for(int k=0;k<4;++k)s.bg[12+k]+=(props[k]-s.bg[12+k])*.018f;
    const char* names[]={"u_bgA","u_bgB","u_bgC","u_bgD"};for(int c=0;c<4;++c)uniform3(s.post,names[c],&s.bg[c*3]);
    glUniform4fv(location(s.post,"u_bgP"),1,&s.bg[12]);glDrawArrays(GL_TRIANGLES,0,3);
    GLuint finished=s.composite,finishedTexture=s.compositeTex;
    // History is captured before the UI and lens, retaining only the form's
    // signed colour relative to its sky. Export redraws never advance history.
    const bool sameView=s.historyPatchView==bindings.visible&&s.historyDark==p.dark;
    if(afterimage<=0)s.historyValid=false;
    if(frameDt>0&&!sameView)s.historyValid=false;
    if(afterimage>0&&(frameDt>0||(s.historyValid&&sameView))){
        s.feedbackBuffers();const int next=1-s.historyIndex;
        glBindFramebuffer(GL_FRAMEBUFFER,s.feedbackTargets[next]);glUseProgram(s.feedback);
        sampler(s.feedback,"b_composite",0,s.compositeTex);sampler(s.feedback,"b_history",1,s.historyTextures[s.historyIndex]);
        glUniform2f(location(s.feedback,"u_resolution"),float(width),float(height));
        glUniform2f(location(s.feedback,"u_feedbackCentre"),.5f+.5f*viewOffset.x,.5f+.5f*viewOffset.y);
        uniform(s.feedback,"u_time",s.time);uniform(s.feedback,"u_dark",s.dark);
        uniform(s.feedback,"u_feedbackDt",std::clamp(frameDt,0.f,.1f));uniform(s.feedback,"u_persistence",afterimage);
        uniform(s.feedback,"u_feedbackDrift",.0125f);uniform(s.feedback,"u_feedbackHit",std::clamp(f.beat*reactivity,0.f,1.f));
        glUniform1i(location(s.feedback,"u_historyValid"),s.historyValid?1:0);
        for(int c=0;c<4;++c)uniform3(s.feedback,names[c],&s.bg[c*3]);
        glUniform4fv(location(s.feedback,"u_bgP"),1,&s.bg[12]);glDrawArrays(GL_TRIANGLES,0,3);
        s.historyIndex=next;s.historyValid=true;s.historyPatchView=bindings.visible;s.historyDark=p.dark;
        finished=s.feedbackTargets[next];finishedTexture=s.feedbackTexture;
    }
    if(p.dof){
        if(!s.lensTarget){
            s.lensTexture=texture(width,height,GL_RGBA16F);glGenFramebuffers(1,&s.lensTarget);
            glBindFramebuffer(GL_FRAMEBUFFER,s.lensTarget);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s.lensTexture,0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU lens framebuffer incomplete");
        }
        glBindFramebuffer(GL_FRAMEBUFFER,s.lensTarget);glUseProgram(s.dof);
        sampler(s.dof,"b_scene",0,s.colour);sampler(s.dof,"b_composite",1,finishedTexture);
        glUniform2f(location(s.dof,"u_resolution"),float(width),float(height));uniform(s.dof,"u_pixelScale",1.f);
        uniform(s.dof,"u_focus",std::max(1.f,Vector3Distance(camera.position,camera.target)+p.focus));
        uniform(s.dof,"u_aperture",p.aperture);uniform(s.dof,"u_maxBlur",p.maxBlur);uniform(s.dof,"u_dpr",1);glDrawArrays(GL_TRIANGLES,0,3);
        finished=s.lensTarget;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,finished);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    glBlitFramebuffer(0,0,width,height,0,0,GetRenderWidth(),GetRenderHeight(),GL_COLOR_BUFFER_BIT,GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,GetRenderWidth(),GetRenderHeight());
    glUseProgram(0);glBindVertexArray(0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,0);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_DEPTH_TEST);
}
void Renderer::finish(){glFinish();}
float Renderer::simulationTime() const{return state->time;}
std::array<Color,20> Renderer::palettePreview() const {
    std::array<Color,20> result{};
    const auto* p=state->paletteData.data()+state->slot*28*16;
    for(size_t i=0;i<result.size();++i){
        const float t=float(i)/(result.size()-1)*p[3]+p[7];
        unsigned char rgb[3];
        for(int k=0;k<3;++k)rgb[k]=static_cast<unsigned char>(255*std::clamp(p[15]+p[11]*(p[k]+p[4+k]*std::cos(6.2831853f*(p[8+k]*t+p[12+k]))),0.f,1.f));
        result[i]={rgb[0],rgb[1],rgb[2],255};
    }
    return result;
}
bool Renderer::validate(std::string& report) {
    auto& s=*state;std::vector<float> positions(maxParticles*4);
    glBindBuffer(GL_ARRAY_BUFFER,s.buffer[s.current][0]);glGetBufferSubData(GL_ARRAY_BUFFER,0,positions.size()*sizeof(float),positions.data());
    int alive=0;bool finite=true;
    for(int i=0;i<maxParticles;++i){if(positions[i*4+3]>=0)++alive;for(int c=0;c<4;++c)finite&=std::isfinite(positions[i*4+c]);}
    float skinError=0;
    if(s.bindingsActive){
        std::vector<float> tissue(s.bindingCount*4);
        for(int k=0;k<3;++k){
            glBindBuffer(GL_ARRAY_BUFFER,s.bindingBuffers[k]);glGetBufferSubData(GL_ARRAY_BUFFER,0,tissue.size()*sizeof(float),tissue.data());
            for(float v:tissue)finite&=std::isfinite(v);
            if(k<2)for(int i=0;i<s.bindingCount;++i)finite&=tissue[i*4+3]>0;
        }
        finite&=s.rootProjectionError<.15f;
        std::array<float,6*bindingSamples*4> samples{};
        glBindBuffer(GL_ARRAY_BUFFER,s.anchorBuffers[1]);glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(samples),samples.data());
        for(int i=0;i<6*bindingSamples;++i){
            for(int k=0;k<4;++k)finite&=std::isfinite(samples[i*4+k]);
            skinError=std::max(skinError,std::abs(samples[i*4+3]+.45f));
        }
        finite&=skinError<2;
    }
    float historyPeak=0;
    if(s.historyValid){
        std::vector<float> history(size_t(s.width)*s.height*4);
        glBindTexture(GL_TEXTURE_2D,s.historyTextures[s.historyIndex]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_FLOAT,history.data());
        for(float v:history){finite&=std::isfinite(v);historyPeak=std::max(historyPeak,std::abs(v));}
        finite&=historyPeak<4;
    }
    auto error=glGetError();report="GPU: "+std::to_string(alive)+" live prisms of "+std::to_string(s.activeCount)+" active; finite="+(finite?"yes":"no")+"; GL error="+std::to_string(error);
    if(s.bindingsActive)report+="; living segments="+std::to_string(s.bindingCount)+"; port projection error="+std::to_string(s.rootProjectionError)+" px; skin fit error="+std::to_string(skinError);
    if(s.historyValid)report+="; history peak="+std::to_string(historyPeak);
    glBindBuffer(GL_ARRAY_BUFFER,0);std::string gradientReport;
    bool gradientValid=verifyGradient(s.simulationSource,gradientReport);report+=gradientReport;
    std::string feedbackReport;const bool feedbackValid=verifyFeedback(s.feedback,s.triangle,s.fullTriangle,s.postPrefix,feedbackReport);report+=feedbackReport;
    return finite&&alive>s.activeCount*.9f&&error==GL_NO_ERROR&&gradientValid&&feedbackValid&&glGetError()==GL_NO_ERROR;
}
}
