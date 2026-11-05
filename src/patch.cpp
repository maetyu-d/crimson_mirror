#include "patch.hpp"
#include "response.hpp"
#include <algorithm>
#include <cmath>

namespace prism {
namespace {
constexpr float nodeWidth=172,worldWidth=1600,worldHeight=720;
const Color ink{243,219,225,255},quiet{169,122,139,255};
const Color audioColour{227,65,96,255},dataColour{208,116,142,255},geometryColour{222,166,178,255};
enum NodeId {Source,Gain,Fft,Envelope,Smooth,Response,Clock,Shape,Curl,Integrate,Prisms,Material,FormTime,SlowLfo,MorphForm,Feedback};
Color alpha(Color c,float opacity){c.a=static_cast<unsigned char>(std::clamp(opacity,0.f,1.f)*255);return c;}
Vector2 bezier(Vector2 a,Vector2 b,Vector2 c,Vector2 d,float t){
    const float u=1-t;
    return {u*u*u*a.x+3*u*u*t*b.x+3*u*t*t*c.x+t*t*t*d.x,u*u*u*a.y+3*u*u*t*b.y+3*u*t*t*c.y+t*t*t*d.y};
}
}
PatchView::PatchView(){resetLayout();}
void PatchView::resetLayout(){
    nodes={{
        {{24,34},142,"Audio"},
        {{24,258},100,"Sensitivity"},
        {{230,34},142,"Spectrum"},
        {{230,258},154,"Envelope"},
        {{436,34},156,"Smooth"},
        {{642,34},144,"Response"},
        {{24,504},94,"Clock"},
        {{230,504},108,"Container"},
        {{436,308},142,"Curl"},
        {{642,308},142,"Integrate"},
        {{848,308},160,"Prisms"},
        {{848,34},142,"Material"},
        {{436,518},100,"Evolution"},
        {{642,518},144,"LFO"},
        {{848,518},174,"Morph"},
        {{848,214},78,"Feedback"}
    }};
    dragging=-1;
}
Rectangle PatchView::bounds(int i) const {
    const auto& n=nodes[i];return {origin.x+n.position.x*scale,origin.y+n.position.y*scale,nodeWidth*scale,n.height*scale};
}
void PatchView::update(const Features& raw,const Features&,float dt){
    historyTime+=dt;
    while(historyTime>=1/30.f){historyTime-=1/30.f;energy[historyCursor]=raw.energy;onsets[historyCursor]=raw.beat;historyCursor=(historyCursor+1)%int(energy.size());}
}
bool PatchView::interact(int width,int height,bool enabled,bool visible){
    scale=std::min((width-40)/worldWidth,(height-110)/worldHeight);
    origin={(width-worldWidth*scale)*.5f,std::max(40.f,(height-74-worldHeight*scale)*.5f)};
    if(!visible||!enabled){dragging=-1;hovered=-1;return false;}
    if(IsKeyPressed(KEY_HOME)){resetLayout();selected=-1;}
    auto mouse=GetMousePosition();hovered=-1;
    for(int i=int(nodes.size())-1;i>=0;--i)if(CheckCollisionPointRec(mouse,bounds(i))){hovered=i;break;}
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
        selected=hovered;
        if(hovered>=0&&mouse.y<bounds(hovered).y+32*scale){
            dragging=hovered;dragOffset={(mouse.x-origin.x)/scale-nodes[hovered].position.x,(mouse.y-origin.y)/scale-nodes[hovered].position.y};
        }
    }
    if(dragging>=0){
        auto& n=nodes[dragging];
        n.position={std::clamp((mouse.x-origin.x)/scale-dragOffset.x,4.f,worldWidth-nodeWidth-4),
                    std::clamp((mouse.y-origin.y)/scale-dragOffset.y,12.f,worldHeight-n.height)};
        if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))dragging=-1;
        return true;
    }
    return hovered>=0;
}
SceneBindings PatchView::sceneBindings(bool visible) const {
    SceneBindings result;result.visible=visible;
    const int ids[]={Prisms,Material};const float y[]={141,124};
    for(int i=0;i<2;++i){const auto r=bounds(ids[i]);result.roots[i]={r.x+r.width,r.y+y[i]*scale};}
    return result;
}
void PatchView::draw(Font font,const Analyzer& analyser,const Features& f,const EvolvingForm& form,
                     float gain,float strength,float smoothing,float simTime,bool frozen,float afterimage,
                     const std::array<Color,20>& palette,Texture2D webcam,int width,int height) const {
    auto point=[&](float x,float y){return Vector2{origin.x+x*scale,origin.y+y*scale};};
    auto rect=[&](float x,float y,float w,float h,Color c){DrawRectangleRec({origin.x+x*scale,origin.y+y*scale,w*scale,h*scale},c);};
    const auto& look=form.look;
    const float inputActivity=std::clamp(analyser.value.rms*8.f,0.f,1.f);
    const float motion=soundMotion(f,strength);
    const float brightness=std::max(.18f,1+strength*(-.65f+f.energy*.95f+f.beat*.9f));
    const float evolutionActivity=form.enabled&&!frozen?.55f:0.f;
    // Let the patch dissolve before the exposed surface grafts.
    const int dissolve=int(origin.x+1010*scale);
    DrawRectangle(0,0,dissolve,height,{9,3,8,142});
    DrawRectangleGradientH(dissolve,0,std::min(width-dissolve,int(180*scale)),height,{9,3,8,142},{9,3,8,0});
    for(int y=14;y<height-66;y+=24)for(int x=16;x<width;x+=24){
        const float opacity=std::clamp((origin.x+1140*scale-x)/(130*scale),0.f,1.f);
        DrawCircle(x,y,.7f,alpha({128,57,78,255},opacity*.196f));
    }
    struct Cable {int from,to;float outY,inY,activity;Color colour;};
    const Cable cables[]={
        {Source,Fft,126,107,inputActivity,audioColour},{Source,Envelope,126,98,inputActivity,audioColour},
        {Gain,Fft,84,125,gain/16,dataColour},{Gain,Envelope,84,134,gain/16,dataColour},
        {Fft,Envelope,107,116,analyser.value.beat,dataColour},
        {Fft,Smooth,125,120,analyser.value.energy,dataColour},{Envelope,Smooth,134,138,analyser.value.beat,dataColour},
        {Smooth,Response,140,126,f.energy,dataColour},{Smooth,Material,140,124,f.energy,dataColour},
        {Response,Curl,126,88,f.beat,dataColour},{Response,Integrate,126,123,frozen?0:motion/4,dataColour},
        {Response,Clock,126,78,frozen?0:motion/4,dataColour},
        {Response,Prisms,126,123,f.energy,dataColour},{Response,Material,126,106,f.beat,dataColour},
        {Clock,Curl,78,106,frozen?0:motion/4,geometryColour},{Clock,Integrate,78,105,frozen?0:motion/4,geometryColour},
        {Shape,MorphForm,92,132,.32f,geometryColour},{Curl,Integrate,124,87,frozen?0:motion/4,geometryColour},
        {Integrate,Prisms,123,141,frozen?0:motion/4,geometryColour},
        {FormTime,SlowLfo,84,108,evolutionActivity,dataColour},{SlowLfo,MorphForm,128,150,evolutionActivity,dataColour},
        {MorphForm,Curl,150,124,evolutionActivity,geometryColour},{MorphForm,Prisms,132,105,evolutionActivity,dataColour},
        {Prisms,Feedback,141,60,afterimage>0?.35f:0,geometryColour},
        {Material,Feedback,124,44,afterimage>0?.35f:0,audioColour},
        {Response,Feedback,126,60,afterimage>0?f.beat:0,dataColour}
    };
    // Curves and moving packets encode activity, not invented audio or beats.
    for(size_t i=0;i<std::size(cables);++i){
        const auto& c=cables[i];const auto& a=nodes[c.from].position;const auto& b=nodes[c.to].position;
        Vector2 start=point(a.x+nodeWidth,a.y+c.outY),end=point(b.x,b.y+c.inY);
        const float bend=std::max(24.f,std::abs(end.x-start.x)*.42f);
        Vector2 p{std::min(start.x+bend,origin.x+(worldWidth-4)*scale),start.y};
        Vector2 q{std::max(end.x-bend,origin.x+4*scale),end.y};
        const bool highlight=selected==c.from||selected==c.to||hovered==c.from||hovered==c.to;
        const float level=std::clamp(c.activity,0.f,1.f);
        DrawSplineSegmentBezierCubic(start,p,q,end,(highlight?2.f:1.25f)*scale,alpha(c.colour,.22f+level*.5f+(highlight?.23f:0)));
        if(level>.002f){
            const float t=std::fmod(float(GetTime())*(.18f+level*.35f)+float(i)*.137f,1.f);
            DrawCircleV(bezier(start,p,q,end,t),(1.4f+level)*scale,alpha(c.colour,.35f+.65f*level));
        }
    }
    // The return wire exposes the actual previous-frame loop without captions.
    const auto fb=nodes[Feedback].position;
    const auto loopStart=point(fb.x+nodeWidth,fb.y+60),loopBottom=point(fb.x+nodeWidth*.5f,fb.y+88),loopEnd=point(fb.x,fb.y+60);
    const Color loopColour=alpha(dataColour,afterimage>0?.55f:.18f);
    DrawSplineSegmentBezierCubic(loopStart,point(fb.x+nodeWidth+48,fb.y+60),point(fb.x+nodeWidth+48,fb.y+88),loopBottom,scale,loopColour);
    DrawSplineSegmentBezierCubic(loopBottom,point(fb.x-48,fb.y+88),point(fb.x-48,fb.y+60),loopEnd,scale,loopColour);
    for(int i=0;i<int(nodes.size());++i){
        const auto r=bounds(i);const auto& n=nodes[i];const bool focus=selected==i||hovered==i;
        DrawRectangleRec({r.x+3*scale,r.y+5*scale,r.width,r.height},{0,0,0,85});
        DrawRectangleRec(r,{20,10,18,242});
        DrawRectangleLinesEx(r,scale,focus?Color{208,101,133,255}:Color{91,43,63,255});
        rect(n.position.x,n.position.y,nodeWidth,30,focus?Color{62,22,40,255}:Color{43,18,32,255});
        DrawTextEx(font,n.title,point(n.position.x+10,n.position.y+8),13*scale,.1f*scale,ink);
        const float activity=i==Feedback?(afterimage>0?f.beat:0):i>=FormTime?evolutionActivity:i==Source?inputActivity:i==Fft||i==Envelope?analyser.value.energy:i==Smooth||i==Response||i==Material?f.energy:frozen?0:motion/4;
        DrawCircleV(point(n.position.x+nodeWidth-11,n.position.y+15),2.5f*scale,alpha(audioColour,.25f+activity*.75f));
    }
    auto trace=[&](int node,const auto& samples,float x,float y,float w,float h,Color c){
        auto p=nodes[node].position;
        for(size_t k=1;k<samples.size();++k){
            const size_t a=(historyCursor+k-1)%samples.size(),b=(historyCursor+k)%samples.size();
            DrawLineEx(point(p.x+x+(k-1)*w/(samples.size()-1),p.y+y+h*(1-samples[a])),
                       point(p.x+x+k*w/(samples.size()-1),p.y+y+h*(1-samples[b])),1.2f*scale,c);
        }
    };
    // Real stereo samples, with a fixed display gain rather than a fake waveform.
    const auto samples=analyser.scope();auto source=nodes[Source].position;
    for(int channel=0;channel<2;++channel){
        const float centre=source.y+65+channel*30;
        DrawLineEx(point(source.x+10,centre),point(source.x+162,centre),scale,{64,28,44,255});
        for(size_t k=1;k<samples.size();++k){
            auto sample=[&](size_t at){return std::clamp((channel?samples[at].r:samples[at].l)*2.5f,-1.f,1.f);};
            DrawLineEx(point(source.x+10+(k-1)*152.f/127,centre-sample(k-1)*10),point(source.x+10+k*152.f/127,centre-sample(k)*10),scale,channel?dataColour:audioColour);
        }
    }
    // Compact signal monitors replace the diagnostic text.
    auto meter=[&](int node,float y,float level,Color colour=dataColour){
        const auto p=nodes[node].position;
        rect(p.x+10,p.y+y,152,3,{68,29,46,255});
        rect(p.x+10,p.y+y,152*std::clamp(level,0.f,1.f),3,colour);
    };
    meter(Gain,57,sensitivityRange.toPosition(gain));
    const auto fft=nodes[Fft].position;
    for(int i=0;i<28;++i){
        const float h=std::clamp(analyser.value.bands[i],0.f,1.f)*65;
        rect(fft.x+10+i*5.5f,fft.y+112-h,3.5f,std::max(1.f,h),alpha(audioColour,.25f+.75f*analyser.value.bands[i]));
    }
    trace(Envelope,energy,10,48,152,65,dataColour);
    trace(Envelope,onsets,10,48,152,65,audioColour);
    meter(Smooth,57,f.bass);meter(Smooth,78,f.mid);meter(Smooth,99,f.high);
    meter(Smooth,115,smoothing/2000,quiet);
    meter(Response,64,motion/4);
    DrawCircleV(point(nodes[Response].position.x+86,nodes[Response].position.y+95),
                (3+9*std::clamp(f.beat,0.f,1.f))*scale,alpha(audioColour,.25f+.75f*f.beat));
    auto clock=[&](int node,double seconds,bool running){
        const auto p=nodes[node].position;const auto centre=point(p.x+86,p.y+60);
        DrawCircleLinesV(centre,18*scale,{68,29,46,255});
        const float phase=float(std::fmod(seconds,60.)/60.)*6.2831853f-1.5707963f;
        DrawLineEx(centre,{centre.x+std::cos(phase)*16*scale,centre.y+std::sin(phase)*16*scale},1.5f*scale,running?dataColour:quiet);
    };
    clock(Clock,simTime,!frozen);clock(FormTime,form.seconds,form.enabled&&!frozen);
    auto shapeIcon=[&](int node,int shape,Color colour){
        const auto p=nodes[node].position;const auto c=point(p.x+86,p.y+66);
        if(shape==0)DrawRectangleLinesEx({c.x-32*scale,c.y-15*scale,64*scale,30*scale},scale,colour);
        else if(shape==1)DrawCircleLinesV(c,24*scale,colour);
        else if(shape==2){DrawEllipseLines(int(c.x),int(c.y),32*scale,21*scale,colour);DrawEllipseLines(int(c.x),int(c.y),15*scale,9*scale,colour);}
        else if(shape==3)DrawRectangleRoundedLinesEx({c.x-34*scale,c.y-17*scale,68*scale,34*scale},1,12,scale,colour);
        else for(int i=1;i<48;++i){
            auto wave=[&](int k){return point(p.x+52+k*68.f/47,p.y+66-13*std::sin(k*6.2831853f/47));};
            DrawLineEx(wave(i-1),wave(i),scale,colour);
        }
    };
    shapeIcon(Shape,form.baseShape,dataColour);
    meter(Curl,61,look.flowScale/.4f);meter(Curl,82,look.flowSpeed/10);
    // Filled points show the actual particle budget, without a numeric counter.
    const auto integrate=nodes[Integrate].position;
    for(int i=0;i<60;++i)DrawCircleV(point(integrate.x+20+(i%15)*9.4f,integrate.y+52+(i/15)*13),1.5f*scale,
                                  i*maxParticles/60<look.count?dataColour:Color{68,29,46,255});
    // The prism glyph follows the current size and stretch.
    const auto prism=nodes[Prisms].position;
    const float halfWidth=std::clamp(look.size*80,8.f,36.f),halfHeight=std::clamp(look.stretch*15,6.f,31.f);
    const Vector2 corners[]={point(prism.x+86-halfWidth,prism.y+77-halfHeight),point(prism.x+86+halfWidth,prism.y+77-halfHeight),
                             point(prism.x+86+halfWidth,prism.y+77+halfHeight),point(prism.x+86-halfWidth,prism.y+77+halfHeight)};
    const Vector2 offset{9*scale,-8*scale};
    for(int i=0;i<4;++i){
        auto a=corners[i],b=corners[(i+1)%4];DrawLineEx(a,b,scale,dataColour);
        DrawLineEx({a.x+offset.x,a.y+offset.y},{b.x+offset.x,b.y+offset.y},scale,quiet);
        DrawLineEx(a,{a.x+offset.x,a.y+offset.y},scale,quiet);
    }
    const auto lfo=nodes[SlowLfo].position;
    for(int i=1;i<76;++i){
        auto wave=[&](int k){return point(lfo.x+10+k*152.f/75,lfo.y+75-(form.enabled?24*std::sin(6.2831853f*(float(k)/75+float(std::fmod(form.seconds,breathCycleSeconds)/breathCycleSeconds))):0));};
        DrawLineEx(wave(i-1),wave(i),scale,form.enabled?dataColour:quiet);
    }
    shapeIcon(MorphForm,look.shape,alpha(dataColour,1-look.shapeBlend));
    shapeIcon(MorphForm,look.nextShape,alpha(dataColour,look.shapeBlend));
    meter(MorphForm,112,look.shapeBlend);meter(MorphForm,121,(form.breath-.89f)/.22f,quiet);
    // The curve is the same exponential decay used by the GPU history.
    for(int i=1;i<76;++i){
        auto decay=[&](int k){return point(fb.x+10+k*152.f/75,fb.y+67-(afterimage>0?24*std::exp(-3.f*k/(75*afterimage)):0));};
        DrawLineEx(decay(i-1),decay(i),scale,afterimage>0?dataColour:quiet);
    }
    // The swatch samples the same palette and brightness as the material.
    const auto material=nodes[Material].position;
    if(webcam.id){
        constexpr float aspect=152.f/42.f;
        const float cropWidth=std::min(float(webcam.width),webcam.height*aspect);
        const float cropHeight=cropWidth/aspect;
        const Rectangle source{(webcam.width-cropWidth)*.5f,(webcam.height-cropHeight)*.5f,-cropWidth,cropHeight};
        const auto position=point(material.x+10,material.y+55);
        DrawTexturePro(webcam,source,{position.x,position.y,152*scale,42*scale},{},0,WHITE);
    }else for(int i=0;i<20;++i){
        Color c=palette[i];c.r=static_cast<unsigned char>(std::min(255.f,c.r*brightness));c.g=static_cast<unsigned char>(std::min(255.f,c.g*brightness));c.b=static_cast<unsigned char>(std::min(255.f,c.b*brightness));
        rect(material.x+10+i*7.6f,material.y+55,7.6f,42,c);
    }
    // Ports remain visible without text labels.
    auto pin=[&](int i,float y,bool outputPin,Color colour){
        auto p=nodes[i].position;auto at=point(p.x+(outputPin?nodeWidth:0),p.y+y);
        if(!(outputPin&&(i==Prisms||i==Material))){DrawCircleV(at,4*scale,{20,10,18,255});DrawCircleV(at,2.4f*scale,colour);}
    };
    pin(Source,126,true,audioColour);pin(Gain,84,true,dataColour);
    pin(Fft,107,false,audioColour);pin(Fft,125,false,dataColour);pin(Fft,107,true,dataColour);pin(Fft,125,true,dataColour);
    pin(Envelope,98,false,audioColour);pin(Envelope,116,false,dataColour);pin(Envelope,134,false,dataColour);pin(Envelope,134,true,dataColour);
    pin(Smooth,120,false,dataColour);pin(Smooth,138,false,dataColour);pin(Smooth,140,true,dataColour);
    pin(Response,126,false,dataColour);pin(Response,126,true,dataColour);
    pin(Clock,78,false,dataColour);pin(Clock,78,true,geometryColour);pin(Shape,92,true,geometryColour);
    pin(Curl,88,false,dataColour);pin(Curl,106,false,geometryColour);pin(Curl,124,false,geometryColour);pin(Curl,124,true,geometryColour);
    pin(Integrate,87,false,geometryColour);pin(Integrate,105,false,geometryColour);pin(Integrate,123,false,dataColour);pin(Integrate,123,true,geometryColour);
    pin(Prisms,105,false,dataColour);pin(Prisms,123,false,dataColour);pin(Prisms,141,false,geometryColour);pin(Prisms,141,true,geometryColour);
    pin(Material,106,false,dataColour);pin(Material,124,false,dataColour);pin(Material,124,true,audioColour);
    pin(FormTime,84,true,dataColour);pin(SlowLfo,108,false,dataColour);pin(SlowLfo,128,true,dataColour);
    pin(MorphForm,132,false,geometryColour);pin(MorphForm,150,false,dataColour);pin(MorphForm,132,true,dataColour);pin(MorphForm,150,true,geometryColour);
    pin(Feedback,44,false,audioColour);pin(Feedback,60,false,geometryColour);pin(Feedback,60,true,dataColour);
}
}
