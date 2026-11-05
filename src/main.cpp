#include "audio.hpp"
#include "renderer.hpp"
#include "webcam.hpp"
#include "response.hpp"
#include "patch.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace prism;
namespace {
const Color ink{246,226,231,255},muted{177,131,145,255},accent{222,65,93,255};
const Color line{71,29,44,255},panel{20,8,15,244};
Font font{},mono{};
void text(const std::string& s,float x,float y,float size,Color colour=ink,bool fixed=false) {
    DrawTextEx(fixed?mono:font,s.c_str(),{x,y},size,fixed?.6f:.15f,colour);
}
float measure(const std::string& s,float size,bool fixed=false){return MeasureTextEx(fixed?mono:font,s.c_str(),size,fixed?.6f:.15f).x;}
std::string fit(std::string s,float width,float size) {
    if(measure(s,size)<=width)return s;
    while(s.size()>1&&measure(s+"...",size)>width)s.pop_back();return s+"...";
}
std::string clockText(double seconds) {
    int t=int(std::max(0.,seconds));char b[40];std::snprintf(b,sizeof b,"%d:%02d",t/60,t%60);return b;
}
bool saveScreenshot(const fs::path& path) {
    rlDrawRenderBatchActive();
    Image image=LoadImageFromScreen();
    bool ok=ExportImage(image,path.string().c_str());UnloadImage(image);return ok;
}
bool contains(Rectangle r){return CheckCollisionPointRec(GetMousePosition(),r);}
void box(Rectangle r,Color colour,float round=.18f){DrawRectangleRounded(r,round,8,colour);}
struct Ui {
    float x,y,w=252;
    bool blocked=false;
    int activeSlider=0;
    float sliderMouseX=0;
    bool click(Rectangle r) const {return !blocked&&contains(r)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);}
    bool buttonAt(Rectangle r,const std::string& label,bool selected=false,bool enabled=true)const {
        bool hover=!blocked&&contains(r);
        box(r,selected?Color{79,24,42,255}:hover?Color{52,24,36,255}:Color{31,13,24,255});
        DrawRectangleRoundedLinesEx(r,.18f,8,1,selected?Color{150,42,68,255}:line);
        text(label,r.x+(r.width-measure(label,14))/2,r.y+(r.height-17)/2,14,enabled?(selected?accent:ink):muted);
        return enabled&&click(r);
    }
    bool choice(const std::string& label,const std::string& value) {
        Rectangle r{x,y,w,28};
        box(r,{33,14,25,255});DrawRectangleRoundedLinesEx(r,.18f,8,1,contains(r)?Color{124,57,78,255}:line);
        text(fit(label+"  "+value,w-32,13),x+9,y+6,13);text("v",x+w-17,y+5,13,muted);
        y+=36;return click(r);
    }
    void slider(int id,const std::string& label,float& value,float low,float high,const std::string& display,float curve=1,bool compact=false) {
        text(label,x,y,13,muted);text(display,x+w-measure(display,12,true),y,12,ink,true);y+=compact?22:27;
        Rectangle hit{x,y-8,w,20};
        const ControlRange range{low,high,curve};
        if(click(hit)){activeSlider=id;sliderMouseX=float(GetMouseX());}
        if(activeSlider==id&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
            const float mouse=float(GetMouseX());
            bool fine=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
            float position=fine?range.toPosition(value)+(mouse-sliderMouseX)/(w*10):(mouse-x)/w;
            value=range.fromPosition(position);sliderMouseX=mouse;
        }
        DrawLineEx({x,y},{x+w,y},3,line);
        const float k=range.toPosition(value);
        DrawLineEx({x,y},{x+w*k,y},3,accent);DrawCircleV({x+w*k,y},4.5f,ink);y+=compact?17:20;
    }
    bool toggle(const std::string& label,bool& value) {
        Rectangle r{x,y,w,24};text(label,x,y+2,13,muted);
        box({x+w-32,y+2,32,17},value?Color{142,29,56,255}:Color{54,25,38,255},1);
        DrawCircleV({x+w-(value?8.f:24.f),y+10.5f},5.5f,ink);y+=27;
        if(click(r)){value=!value;return true;}return false;
    }
};
std::string decimal(float value,int precision=1){char b[64];std::snprintf(b,sizeof b,"%.*f",precision,value);return b;}
std::string multiplier(float value){return value==0?"Off":decimal(value,value<.1f?4:value<1?3:2)+"x";}
enum class Popup {None,Preset,Palette,Shape,Input};
std::string assetPath() {
    fs::path executable=GetApplicationDirectory();
    for(const auto& p:{executable/"../Resources/assets",executable/"assets",fs::path(PRISM_SOURCE_ASSETS)})
        if(fs::exists(p/"shaders/prism.vert"))return p.lexically_normal().string();
    throw std::runtime_error("Cannot find Prism Currents assets");
}
bool verifyVisualResponse(Renderer& renderer,const Look& look,const fs::path& destination) {
    Analyzer probe;AudioRing ring;std::array<float,1600> samples{};
    Features quiet,hit;float low=4,high=0;
    // Find a real trough and transient in the built-in audio, then hold the
    // particle state fixed so image differences come from audio modulation.
    for(int frame=0;frame<480;++frame){
        synthesize(samples.data(),800,uint64_t(frame)*800);ring.push(samples.data(),800);probe.update(ring,1/60.f);
        if(frame<60)continue;
        float motion=soundMotion(probe.value,renderer.reactivity);
        if(motion<low){low=motion;quiet=probe.value;}
        if(motion>high){high=motion;hit=probe.value;}
    }
    auto capture=[&](const Features& features){
        BeginDrawing();ClearBackground({17,5,11,255});
        renderer.draw(look,features,GetScreenWidth(),GetScreenHeight());
        rlDrawRenderBatchActive();Image frame=LoadImageFromScreen();EndDrawing();return frame;
    };
    for(int i=0;i<30;++i){Image settle=capture(quiet);UnloadImage(settle);}
    Image a=capture(quiet),b=capture(hit);
    if(!IsImageValid(a)||!IsImageValid(b)||a.width!=b.width||a.height!=b.height){UnloadImage(a);UnloadImage(b);return false;}
    auto* pixelsA=static_cast<unsigned char*>(a.data);auto* pixelsB=static_cast<unsigned char*>(b.data);
    double brightnessA=0,brightnessB=0;size_t changed=0;
    const size_t pixels=size_t(a.width)*a.height;
    for(size_t i=0;i<pixels;++i){
        auto at=i*4;int delta=0;
        for(int c=0;c<3;++c)delta+=std::abs(int(pixelsA[at+c])-int(pixelsB[at+c]));
        if(delta>18)++changed;
        brightnessA+=.2126*pixelsA[at]+.7152*pixelsA[at+1]+.0722*pixelsA[at+2];
        brightnessB+=.2126*pixelsB[at]+.7152*pixelsB[at+1]+.0722*pixelsB[at+2];
    }
    double ratio=brightnessB/std::max(1.,brightnessA),fraction=double(changed)/pixels;
    bool saved=true;
    if(!destination.empty()){
        fs::path output=fs::absolute(destination);fs::create_directories(output.parent_path());
        auto quietFile=output.parent_path()/(output.stem().string()+"-quiet.png");
        saved=ExportImage(a,quietFile.string().c_str())&&ExportImage(b,output.string().c_str());
    }
    UnloadImage(a);UnloadImage(b);
    std::string report;bool valid=renderer.validate(report);std::cout<<report<<"\n";
    std::cout<<"Visual response: motion="<<low<<".."<<high<<"x; brightness ratio="<<ratio
        <<"x; changed pixels="<<fraction*100<<"%\n";
    return saved&&valid&&ratio>1.35&&fraction>.12;
}
struct Options {
    std::string file,capture;
    float sensitivity=1,reactivity=1.35f,smoothing=0,afterimage=1.25f;
    double formTime=0;
    bool demo=false,renderTest=false,smoke=false,showUi=false,showControls=false,showLook=false,benchmark=false,responseTest=false,hidePatch=false,noEvolution=false;
    int frames=180,preset=-1,shape=-1;
};
Options options(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i){std::string a=argv[i];
        if(a=="--demo")o.demo=true;
        else if(a=="--render-test")o.renderTest=true;
        else if(a=="--smoke-test")o.smoke=true;
        else if(a=="--show-ui")o.showUi=true;
        else if(a=="--hide-patch")o.hidePatch=true;
        else if(a=="--no-evolution")o.noEvolution=true;
        else if(a=="--form-time"&&i+1<argc)o.formTime=std::atof(argv[++i]);
        else if(a=="--show-controls")o.showControls=true;
        else if(a=="--show-look"){o.showLook=true;o.showControls=true;}
        else if(a=="--benchmark")o.benchmark=true;
        else if(a=="--response-test")o.responseTest=true;
        else if(a=="--sensitivity"&&i+1<argc)o.sensitivity=std::clamp(float(std::atof(argv[++i])),sensitivityRange.minimum,sensitivityRange.maximum);
        else if(a=="--reactivity"&&i+1<argc)o.reactivity=std::clamp(float(std::atof(argv[++i])),reactivityRange.minimum,reactivityRange.maximum);
        else if(a=="--smoothing"&&i+1<argc)o.smoothing=std::clamp(float(std::atof(argv[++i])),smoothingRange.minimum,smoothingRange.maximum);
        else if(a=="--afterimage"&&i+1<argc)o.afterimage=std::clamp(float(std::atof(argv[++i])),0.f,3.f);
        else if(a=="--capture"&&i+1<argc)o.capture=argv[++i];
        else if(a=="--frames"&&i+1<argc)o.frames=std::max(1,std::atoi(argv[++i]));
        else if(a=="--preset"&&i+1<argc)o.preset=std::atoi(argv[++i]);
        else if(a=="--shape"&&i+1<argc)o.shape=std::atoi(argv[++i]);
        else if(a=="--help"){
            std::cout<<"PrismCurrents [audio.wav|audio.mp3|audio.flac] [--demo]\n"
                "  --render-test --frames N --capture /absolute/frame.png [--preset 0..15]\n"
                "  --benchmark  (uncapped timing, including GPU completion; combine with --render-test)\n"
                "  --response-test [--capture hit.png]  (compare demo trough/hit at a fixed particle pose)\n"
                "  --sensitivity 0..16 --reactivity 0..8 --smoothing 0..2000 (milliseconds)\n"
                "  --afterimage 0..3 (seconds; 0 disables feedback)\n"
                "  --show-ui [--hide-patch] [--show-controls | --show-look] (interface captures)\n"
                "  --form-time SECONDS | --no-evolution (evolving form / fixed base form)\n"
                "  --smoke-test --frames N  (decoder/playback integration, audible output muted)\n";std::exit(0);
        }else if(a.rfind("--",0)!=0)o.file=a;
    }
    return o;
}
}
int main(int argc,char** argv) {
    const auto config=options(argc,argv);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIGHDPI|(config.benchmark?0:FLAG_VSYNC_HINT));
    SetTraceLogLevel(LOG_WARNING);InitWindow(1280,780,"Prism Currents");SetWindowMinSize(1000,760);SetExitKey(KEY_NULL);SetTargetFPS(config.benchmark?0:60);
    SetWindowPosition(std::max(0,(GetMonitorWidth(GetCurrentMonitor())-1280)/2),55);
    int result=0;
    try {
        const auto assets=assetPath();
        font=LoadFontEx((assets+"/fonts/Karla-Regular.ttf").c_str(),64,nullptr,0);
        mono=LoadFontEx((assets+"/fonts/Cousine-Regular.ttf").c_str(),32,nullptr,0);
        SetTextureFilter(font.texture,TEXTURE_FILTER_BILINEAR);SetTextureFilter(mono.texture,TEXTURE_FILTER_BILINEAR);
        Renderer renderer(assets);Audio audio;Analyzer analyzer;FeatureSmoother smoother;PatchView patch;FormEvolution evolution(config.formTime);
        Webcam webcam;
        WebcamFrame webcamFrame;
        bool webcamEnabled=false;
        Look look;int preset=-1,scheme=crimsonScheme,shape=0,activeTab=config.showLook?1:0,slot=0;
        uint32_t seed=161301;float gain=config.sensitivity,response=config.reactivity,smoothing=config.smoothing,afterimage=config.afterimage,volume=.65f,paletteClock=0;
        bool uiVisible=true,controlsOpen=config.showControls,patchVisible=!config.hidePatch,orbit=false,freeze=false,mutedAudio=false,help=false,formEvolving=!config.noEvolution;
        Popup popup=Popup::None;int popupScroll=0;
        std::vector<InputDevice> devices;
        std::string message;double messageUntil=0;
        const auto presets=originalPresets();
        auto notify=[&](std::string s){message=std::move(s);messageUntil=GetTime()+6;};
        auto applyPreset=[&](int index){
            preset=index;const auto& p=presets[index];look=p.look;shape=look.shape;seed=p.seed;
            renderer.camera.position={p.position[0],p.position[1],p.position[2]};renderer.camera.target={p.target[0],p.target[1],p.target[2]};
            renderer.palette(scheme,seed,0);renderer.palette(scheme,p.seed2,1);slot=0;paletteClock=0;renderer.useSlot(0);
            renderer.clearFeedback();
        };
        renderer.palette(scheme,seed,0);
        if(config.preset>=0&&config.preset<int(presets.size()))applyPreset(config.preset);
        if(config.shape>=0&&config.shape<5){look.shape=config.shape;shape=config.shape;}
        auto openAudio=[&](const std::string& path){
            if(path.empty())return;
            if(audio.open(path)){analyzer.reset();smoother.reset();activeTab=0;}else notify(audio.error);
        };
        if(!config.file.empty())openAudio(config.file);
        if(config.demo||(config.smoke&&config.file.empty())){if(!audio.demo())notify(audio.error);}
        if(config.smoke){audio.setVolume(0);volume=0;}
        if(config.renderTest)audio.title="Silent test signal / 112 BPM";
        // Fill the routes before the first interactive frame.
        BeginDrawing();ClearBackground({17,5,11,255});EndDrawing();
        const auto initialForm=evolution.sample(look,formEvolving);
        for(int i=0;i<280;++i)renderer.step(1/20.f,initialForm.look,{});
        renderer.reactivity=response;
        renderer.afterimage=afterimage;
        if(config.responseTest)result=verifyVisualResponse(renderer,look,config.capture)?0:2;
        Ui ui;int frame=0;bool screenshotNext=false;
        float largestRms=0,largestBass=0,largestHigh=0,largestEnergy=0;
        float minMotion=4,maxMotion=0;int onsetCount=0;float previousBeat=0;
        std::array<float,1600> testSignal{};uint64_t testFrame=0;
        renderer.finish();
        const auto benchmarkStart=std::chrono::steady_clock::now();
        while(!config.responseTest&&!WindowShouldClose()) {
            const float dt=std::clamp(GetFrameTime(),.001f,.1f);++frame;
            if(config.renderTest){synthesize(testSignal.data(),800,testFrame);testFrame+=800;audio.ring.push(testSignal.data(),800);}
            analyzer.update(audio.ring,config.renderTest?1/60.f:dt,gain);
            const auto& features=smoother.update(analyzer.value,config.renderTest?1/60.f:dt,smoothing);
            patch.update(analyzer.value,features,config.renderTest?1/60.f:dt);
            largestRms=std::max(largestRms,features.rms);largestBass=std::max(largestBass,features.bass);largestHigh=std::max(largestHigh,features.high);
            largestEnergy=std::max(largestEnergy,features.energy);
            if(features.beat>previousBeat+.15f)++onsetCount;previousBeat=features.beat;
            int width=GetScreenWidth(),height=GetScreenHeight();
            ui.blocked=popup!=Popup::None||help;
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))ui.activeSlider=0;
            if(IsKeyPressed(KEY_TAB))uiVisible=!uiVisible;
            if(IsKeyPressed(KEY_G))patchVisible=!patchVisible;
            if(IsKeyPressed(KEY_H))help=!help;
            if(IsKeyPressed(KEY_F))ToggleBorderlessWindowed();
            if(IsKeyPressed(KEY_ESCAPE)){if(popup!=Popup::None)popup=Popup::None;else if(help)help=false;else if(!uiVisible)uiVisible=true;else if(controlsOpen)controlsOpen=false;}
            if(IsKeyPressed(KEY_O)){openAudio(chooseAudioFile());}
            if(IsFileDropped()){auto dropped=LoadDroppedFiles();if(dropped.count>0)openAudio(dropped.paths[0]);UnloadDroppedFiles(dropped);}
            if(IsKeyPressed(KEY_SPACE)){
                if(audio.source==AudioSource::None)freeze=!freeze;else audio.setPlaying(!audio.playing());
            }
            if(IsKeyPressed(KEY_P))freeze=!freeze;
            if(IsKeyPressed(KEY_M))mutedAudio=!mutedAudio;
            if(IsKeyPressed(KEY_D))look.dark=!look.dark;
            if(IsKeyPressed(KEY_A))orbit=!orbit;
            if(IsKeyPressed(KEY_S))screenshotNext=true;
            if(IsKeyPressed(KEY_R)){seed+=19427;renderer.palette(scheme,seed,slot);}
            const Rectangle controlsButton{float(width)-112,float(height)-44,96,28};
            const Rectangle patchButton{float(width)-190,float(height)-44,70,28};
            const float controlsHeight=activeTab==0?(audio.duration()>0?344.f:312.f):533.f;
            const Rectangle controlsPanel{float(width)-260,float(height)-52-controlsHeight,244,controlsHeight};
            bool onPanel=uiVisible&&(contains(controlsButton)||contains(patchButton)||(controlsOpen&&contains(controlsPanel))||ui.activeSlider!=0);
            if(controlsOpen&&!onPanel&&popup==Popup::None&&!help&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))controlsOpen=false;
            const bool patchOwnsPointer=patch.interact(width,height,!onPanel&&popup==Popup::None&&!help,uiVisible&&patchVisible&&(!config.renderTest||config.showUi));
            if(!onPanel&&!patchOwnsPointer&&popup==Popup::None&&!help){
                float radius=Vector3Distance(renderer.camera.position,renderer.camera.target);
                if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
                    const auto delta=GetMouseDelta();Vector3 offset=Vector3Subtract(renderer.camera.position,renderer.camera.target);
                    float yaw=std::atan2(offset.x,offset.z)-delta.x*.006f;
                    float pitch=std::clamp(std::asin(offset.y/radius)+delta.y*.006f,-1.49f,1.49f);
                    renderer.camera.position=Vector3Add(renderer.camera.target,{radius*std::cos(pitch)*std::sin(yaw),radius*std::sin(pitch),radius*std::cos(pitch)*std::cos(yaw)});
                    orbit=false;
                }
                if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)){
                    auto delta=GetMouseDelta();auto forward=Vector3Normalize(Vector3Subtract(renderer.camera.target,renderer.camera.position));auto right=Vector3Normalize(Vector3CrossProduct(forward,renderer.camera.up));auto up=Vector3CrossProduct(right,forward);
                    auto offset=Vector3Add(Vector3Scale(right,-delta.x*radius*.0008f),Vector3Scale(up,delta.y*radius*.0008f));
                    renderer.camera.position=Vector3Add(renderer.camera.position,offset);renderer.camera.target=Vector3Add(renderer.camera.target,offset);
                }
                const float wheel=GetMouseWheelMove();if(wheel!=0){
                    auto offset=Vector3Subtract(renderer.camera.position,renderer.camera.target);float length=std::clamp(radius*std::exp(-wheel*.09f),3.f,140.f);renderer.camera.position=Vector3Add(renderer.camera.target,Vector3Scale(offset,length/radius));
                }
            }
            if(orbit&&!freeze){
                auto offset=Vector3Subtract(renderer.camera.position,renderer.camera.target);float yaw=std::atan2(offset.x,offset.z)+(config.renderTest?1/60.f:dt)*.09f;
                float radial=std::sqrt(offset.x*offset.x+offset.z*offset.z);renderer.camera.position=Vector3Add(renderer.camera.target,{radial*std::sin(yaw),offset.y,radial*std::cos(yaw)});
            }
            renderer.reactivity=response;
            renderer.afterimage=afterimage;
            evolution.update(config.renderTest?1/60.:double(dt),formEvolving,freeze);
            const auto form=evolution.sample(look,formEvolving);
            if(!freeze) {
                float motion=soundMotion(features,response);
                minMotion=std::min(minMotion,motion);maxMotion=std::max(maxMotion,motion);
                float simDt=std::min(.075f,(config.renderTest?1/60.f:std::min(dt,1/30.f))*look.speed*motion);
                renderer.step(simDt,form.look,features);paletteClock+=simDt;
                if(look.secondPalette&&paletteClock>=look.switchTime){slot=1-slot;renderer.useSlot(slot);paletteClock=0;}
            }
            const auto bindings=patch.sceneBindings(uiVisible&&patchVisible&&(!config.renderTest||config.showUi));
            if(webcamEnabled){
                std::string error;
                if(webcam.poll(webcamFrame,error))renderer.uploadWebcamFrame(webcamFrame);
                if(!error.empty()){
                    notify(error);
                    webcamEnabled=false;
                    webcam.setEnabled(false);
                }
            }
            if(!webcamEnabled && renderer.webcamPreview().id){
                renderer.clearWebcam();
                webcamFrame={};
            }
            BeginDrawing();ClearBackground({17,5,11,255});renderer.draw(form.look,features,width,height,bindings,freeze?0:config.renderTest?1/60.f:dt);
            if(uiVisible&&(!config.renderTest||config.showUi)){
                if(patchVisible)patch.draw(font,analyzer,features,form,gain,response,smoothing,renderer.simulationTime(),freeze,afterimage,renderer.palettePreview(),renderer.webcamPreview(),width,height);
                if(controlsOpen){
                    box(controlsPanel,panel,.06f);
                    ui.x=controlsPanel.x+16;ui.y=controlsPanel.y+14;ui.w=212;
                    constexpr float half=103;
                    if(ui.buttonAt({ui.x,ui.y,half,26},"Audio",activeTab==0))activeTab=0;
                    if(ui.buttonAt({ui.x+half+6,ui.y,half,26},"Look",activeTab==1))activeTab=1;
                    ui.y+=36;
                    if(activeTab==0){
                        const float third=(ui.w-12)/3;
                        if(ui.buttonAt({ui.x,ui.y,third,26},"Open"))openAudio(chooseAudioFile());
                        if(ui.buttonAt({ui.x+third+6,ui.y,third,26},"Input",audio.source==AudioSource::Input)){
                            devices=audio.inputs();popup=Popup::Input;popupScroll=0;
                        }
                        if(ui.buttonAt({ui.x+(third+6)*2,ui.y,third,26},"Demo",audio.source==AudioSource::Demo)){
                            if(audio.demo()){analyzer.reset();smoother.reset();}else notify(audio.error);
                        }
                        ui.y+=34;
                        const std::string track=config.renderTest?"Test signal":audio.source==AudioSource::None?"Choose audio":audio.source==AudioSource::Demo?"Demo":audio.title;
                        text(fit(track,ui.w,13),ui.x,ui.y,13,muted);ui.y+=22;
                        if(audio.duration()>0){
                            DrawLineEx({ui.x,ui.y},{ui.x+ui.w,ui.y},2,line);
                            DrawLineEx({ui.x,ui.y},{ui.x+ui.w*float(audio.position()/audio.duration()),ui.y},2,accent);
                            if(ui.click({ui.x,ui.y-5,ui.w,13})){
                                audio.seek(std::clamp((GetMouseX()-ui.x)/double(ui.w),0.,1.)*audio.duration());analyzer.reset();smoother.reset();
                            }
                            text(clockText(audio.position())+" / "+clockText(audio.duration()),ui.x,ui.y+8,10,muted,true);ui.y+=30;
                        }
                        if(ui.buttonAt({ui.x,ui.y,half,26},audio.playing()?"Pause":"Play",audio.playing(),audio.source!=AudioSource::None))audio.setPlaying(!audio.playing());
                        if(ui.buttonAt({ui.x+half+6,ui.y,half,26},mutedAudio?"Unmute":"Mute",mutedAudio,audio.source!=AudioSource::Input))mutedAudio=!mutedAudio;
                        ui.y+=38;
                        ui.slider(1,"Sensitivity",gain,sensitivityRange.minimum,sensitivityRange.maximum,multiplier(gain),sensitivityRange.curve,true);
                        ui.slider(2,"Reactivity",response,reactivityRange.minimum,reactivityRange.maximum,multiplier(response),reactivityRange.curve,true);
                        ui.slider(7,"Smoothing",smoothing,smoothingRange.minimum,smoothingRange.maximum,smoothing==0?"Off":smoothing<10?decimal(smoothing,1)+" ms":std::to_string(int(smoothing))+" ms",smoothingRange.curve,true);
                        ui.slider(3,"Volume",volume,0,1,std::to_string(int(volume*100))+"%",1,true);
                    }else{
                        if(ui.choice("Preset",preset<0?"Default":presets[preset].name)){popup=Popup::Preset;popupScroll=0;}
                        if(ui.choice("Palette",schemes[scheme].name)){popup=Popup::Palette;popupScroll=0;}
                        const bool startingWebcam=webcamEnabled && !renderer.webcamPreview().id;
                        if(ui.toggle(startingWebcam?"Webcam starting":"Webcam texture",webcamEnabled))
                            webcam.setEnabled(webcamEnabled);
                        const char* shapes[]={"Slab","Sphere","Torus","Capsule","Wave"};
                        if(ui.choice("Base form",shapes[shape])){popup=Popup::Shape;popupScroll=0;}
                        ui.slider(4,"Speed",look.speed,.1f,2,decimal(look.speed)+"x",1,true);
                        ui.slider(5,"Size",look.size,.06f,1.2f,decimal(look.size,2),1,true);
                        float count=float(look.count);ui.slider(6,"Prisms",count,3000,100000,std::to_string(look.count/1000)+"k",1,true);look.count=int(count/1000)*1000;
                        ui.slider(8,"Afterimage",afterimage,0,3,afterimage==0?"Off":decimal(afterimage,2)+" s",2,true);
                        ui.toggle("Dark",look.dark);ui.toggle("Orbit",orbit);ui.toggle("Depth of field",look.dof);ui.toggle("Evolve form",formEvolving);ui.toggle("Freeze",freeze);
                        ui.y+=8;
                        if(ui.buttonAt({ui.x,ui.y,half,28},"Colours")){seed+=19427;renderer.palette(scheme,seed,slot);}
                        if(ui.buttonAt({ui.x+half+6,ui.y,half,28},"Reset view")){
                            renderer.camera.position={0,0,58};renderer.camera.target={0,0,0};look.fov=35;orbit=false;renderer.clearFeedback();
                        }
                    }
                }
                if(ui.buttonAt(controlsButton,controlsOpen?"Close":"Controls"))controlsOpen=!controlsOpen;
                if(ui.buttonAt(patchButton,"Patch",patchVisible))patchVisible=!patchVisible;
            }
            if(popup!=Popup::None){
                std::vector<std::string> items;std::string title;
                if(popup==Popup::Preset){title="Choose a preset";items.push_back("Default");for(auto& p:presets)items.push_back(p.name);}
                if(popup==Popup::Palette){title="Choose a palette";for(auto& s:schemes)items.push_back(s.name);}
                if(popup==Popup::Shape){title="Choose a base form";items={"Slab","Sphere","Torus","Capsule","Wave sheet"};}
                if(popup==Popup::Input){title="Choose live input";items.push_back("System default input");for(auto& d:devices)items.push_back(d.name);}
                const int visible=std::min(int(items.size()),int((height-210)/31));
                popupScroll=std::clamp(popupScroll-int(GetMouseWheelMove()),0,std::max(0,int(items.size())-visible));
                float h=visible*31.f+96,w=430,x=(width-w)/2,y=(height-h)/2;
                DrawRectangle(0,0,width,height,{10,2,7,180});box({x,y,w,h},panel,.07f);
                DrawRectangleRoundedLinesEx({x,y,w,h},.07f,8,1,line);text(title,x+20,y+18,20);
                for(int row=0;row<visible;++row){int i=row+popupScroll;Rectangle r{x+12,y+53+row*31,w-24,29};
                    bool hovered=contains(r);if(hovered)box(r,{76,24,43,255});text(fit(items[i],w-52,14),r.x+10,r.y+6,14,hovered?accent:ink);
                    if(hovered&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
                        if(popup==Popup::Preset){
                            if(i==0){look=Look{};shape=0;preset=-1;scheme=crimsonScheme;seed=161301;renderer.palette(scheme,seed,0);renderer.camera.position={0,0,58};renderer.camera.target={0,0,0};slot=0;renderer.useSlot(0);}
                            else applyPreset(i-1);
                            evolution=FormEvolution{};renderer.clearFeedback();
                        }else if(popup==Popup::Palette){scheme=i;renderer.palette(scheme,seed,slot);look.secondPalette=false;}
                        else if(popup==Popup::Shape){shape=i;look.shape=i;evolution=FormEvolution{};}
                        else if(popup==Popup::Input){if(audio.input(i==0?-1:devices[i-1].index)){analyzer.reset();smoother.reset();}else notify(audio.error);}
                        popup=Popup::None;
                    }
                }
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&!contains({x,y,w,h}))popup=Popup::None;
            }
            if(help){
                DrawRectangle(0,0,width,height,{10,2,7,190});float x=(width-550)/2.f,y=(height-510)/2.f;box({x,y,550,510},panel,.08f);
                text("Shortcuts",x+30,y+27,27);
                text("x",x+510,y+29,19,muted);
                const char* keys[]={"O","Space","P","M","G","Home","Tab","F","A","R","D","S","Mouse"};
                const char* actions[]={"Open a WAV, MP3 or FLAC","Play / pause audio","Freeze / resume form and currents","Mute / unmute speakers","Toggle the live node patch","Reset node positions","Hide / show the entire interface","Presentation view","Toggle camera orbit","Roll new colours","Toggle dark background","Save a clean PNG","Drag: orbit / scroll: zoom / right-drag: pan"};
                for(int i=0;i<13;++i){text(keys[i],x+30,y+108+i*25,12,accent,true);text(actions[i],x+118,y+106+i*25,14);}
                text("Shift-drag sliders for fine adjustment.",x+30,y+472,12,muted);
                if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&contains({x+495,y+20,40,35}))help=false;
            }
            if(GetTime()<messageUntil&&!config.renderTest){
                float w=std::min(float(width)-300,640.f);box({16,float(height)-60,w,44},{37,12,24,247});
                text(fit(message,w-24,13),28,float(height)-46,13,accent);
            }
            audio.setVolume(mutedAudio?0:volume);
            bool finalFrame=(config.renderTest||config.smoke||config.benchmark)&&frame>=config.frames;
            // Capture after rendering the currents without the UI on the next frame.
            if(screenshotNext&&!config.renderTest){
                fs::path dest=fs::path(GetApplicationDirectory())/"../../../captures";
                if(!fs::exists(dest))dest=fs::temp_directory_path();
                auto stamp=std::chrono::system_clock::now().time_since_epoch();
                auto path=(dest/("prism-"+std::to_string(std::chrono::duration_cast<std::chrono::seconds>(stamp).count())+".png")).lexically_normal();
                auto chosen=choosePngFile(path.string());
                renderer.draw(form.look,features,width,height); // flush UI, then cover it with the clean scene
                if(!chosen.empty()){
                    if(saveScreenshot(chosen))notify("Saved "+chosen);else notify("Could not save the image. Choose a writable folder.");
                }screenshotNext=false;
            }
            if(finalFrame&&!config.capture.empty()){
                fs::path p=fs::absolute(config.capture);fs::create_directories(p.parent_path());if(!saveScreenshot(p))result=2;
            }
            EndDrawing();
            if(config.benchmark)renderer.finish();
            if(finalFrame){
                if(config.benchmark){
                    double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-benchmarkStart).count();
                    std::cout<<"Benchmark: "<<frame<<" frames; "<<elapsed*1000/frame<<" ms/frame; "<<frame/elapsed<<" FPS; "<<look.count<<" prisms\n";
                }
                std::string report;bool valid=renderer.validate(report);std::cout<<report<<"\n";
                std::cout<<"Audio: peak RMS="<<largestRms<<"; bass="<<largestBass<<"; treble="<<largestHigh<<"\n";
                std::cout<<"Response: energy="<<largestEnergy<<"; onsets="<<onsetCount<<"; motion="<<minMotion<<".."<<maxMotion<<"x\n";
                std::cout<<"Form: "<<shapeName(form.look.shape)<<" -> "<<shapeName(form.look.nextShape)<<"; blend="<<form.look.shapeBlend<<"; clock="<<form.seconds<<" s; breath="<<form.breath<<"x\n";
                if((config.renderTest||config.smoke)&&largestRms<.01f)valid=false;
                if(!valid)result=2;break;
            }
        }
        UnloadFont(font);UnloadFont(mono);
    } catch(const std::exception& e){std::cerr<<"Prism Currents: "<<e.what()<<"\n";result=1;}
    CloseWindow();return result;
}
