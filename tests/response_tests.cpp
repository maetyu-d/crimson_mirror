#include "audio.hpp"
#include "response.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace prism;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void demoResponse(float level) {
    AudioRing ring;Analyzer analyzer;
    std::array<float,1600> samples{};
    float minEnergy=1,maxEnergy=0,minSpeed=4,maxSpeed=0,previousBeat=0;
    int onsets=0;std::array<int,4> groups{};
    for(int frame=0;frame<480;++frame){
        synthesize(samples.data(),800,uint64_t(frame)*800);
        for(auto& sample:samples)sample*=level;
        ring.push(samples.data(),800);analyzer.update(ring,1/60.f);
        const auto& f=analyzer.value;
        if(frame>=60){
            minEnergy=std::min(minEnergy,f.energy);maxEnergy=std::max(maxEnergy,f.energy);
            float speed=soundMotion(f,1.35f);minSpeed=std::min(minSpeed,speed);maxSpeed=std::max(maxSpeed,speed);
            if(f.beat>previousBeat+.15f){++onsets;++groups[frame/120];}
        }
        previousBeat=f.beat;
    }
    std::cout<<"Demo level="<<level<<": energy="<<minEnergy<<".."<<maxEnergy
        <<"; motion="<<minSpeed<<".."<<maxSpeed<<"; onsets="<<onsets<<"\n";
    require(maxEnergy-minEnergy>.25f,"Demo must retain substantial amplitude contrast");
    require(maxSpeed-minSpeed>1.5f&&maxSpeed>minSpeed*2,"Demo must produce pronounced changes in motion");
    require(onsets>=8,"Demo percussion must trigger repeated onsets");
    for(auto count:groups)require(count>0,"Onsets must continue throughout the demo");
}
void controlRanges() {
    for(const auto& range:{sensitivityRange,reactivityRange,smoothingRange}){
        require(range.fromPosition(0)==0,"Each audio control must reach a true zero");
        require(range.fromPosition(1)==range.maximum,"Each audio control must reach its full range");
        float previous=-1;
        for(int i=0;i<=1000;++i){float position=i/1000.f,value=range.fromPosition(position);
            require(value>=previous,"Slider values must increase continuously");
            require(std::abs(range.toPosition(value)-position)<.00001f,"Slider handle must match its value throughout the range");previous=value;
        }
    }
    require(sensitivityRange.toPosition(.1f)>.15f&&reactivityRange.toPosition(.1f)>.2f,"Subtle values must receive substantial slider travel");
    Features moderate;moderate.energy=.45f;
    require(std::abs(soundMotion(moderate,8)-soundMotion(moderate,3))>.05f,"The extended reactivity range must affect motion");
}
void sensitivityTrim() {
    float previousEnergy=-1,previousBeat=-1;
    for(float gain:{0.f,.001f,.01f,.1f,1.f,16.f}){
        Analyzer analyzer;AudioRing ring;std::array<float,1600> samples{};
        float energy=0,beat=0,rms=0;
        for(int frame=0;frame<180;++frame){
            synthesize(samples.data(),800,uint64_t(frame)*800);ring.push(samples.data(),800);analyzer.update(ring,1/60.f,gain);
            energy=std::max(energy,analyzer.value.energy);beat=std::max(beat,analyzer.value.beat);rms=std::max(rms,analyzer.value.rms);
        }
        require(rms>.01f,"Sensitivity must not change the actual input meter");
        require(energy>=previousEnergy&&beat>=previousBeat,"Sensitivity must scale both amplitude and beat cues monotonically");
        if(gain==0)require(energy==0&&beat==0,"Sensitivity Off must suppress every modulation cue");
        if(gain==.001f)require(energy>0&&energy<.01f&&beat>0&&beat<.002f,"Very low sensitivity must remain subtle, including beats");
        std::cout<<"Sensitivity "<<gain<<": energy="<<energy<<"; beat="<<beat<<"\n";
        previousEnergy=energy;previousBeat=beat;
    }
}
void smoothingControl() {
    Features input;input.bands.fill(1);input.energy=input.beat=input.bass=input.mid=input.high=1;
    input.rms=.25f;input.peak=.8f;input.dominantHz=440;
    FeatureSmoother off,slow,faster;
    require(off.update(input,1/60.f,0).beat==1,"Smoothing Off must pass transients through immediately");
    float slowFirst=slow.update(input,1/60.f,2000).energy;
    require(slowFirst>0&&slowFirst<.01f,"Maximum smoothing must begin gently");
    require(slowFirst<faster.update(input,1/60.f,100).energy,"More smoothing must produce a slower response");
    slow.reset();FeatureSmoother highRate;
    for(int i=0;i<30;++i)slow.update(input,1/60.f,500);
    for(int i=0;i<120;++i)highRate.update(input,1/240.f,500);
    const auto& value=slow.update(input,0,500);const auto& other=highRate.update(input,0,500);
    require(value.energy>.62f&&value.energy<.65f,"500 ms smoothing must approach its target gradually");
    require(std::abs(value.energy-other.energy)<.00001f,"Smoothing must be independent of rendering frame rate");
    require(value.beat==value.energy&&value.bands[0]==value.energy&&value.bass==value.energy,"Smoothing must cover beats, bands and motion together");
    require(value.rms==input.rms&&value.peak==input.peak&&value.dominantHz==440,"Input meters must remain unsmoothed");
    Features silence;
    for(int i=0;i<120;++i)slow.update(silence,1/60.f,500);
    require(slow.update(silence,0,500).energy<.02f,"Smoothing must release towards silence");
    slow.reset();require(slow.update(silence,1/60.f,2000).beat==0,"Source changes must clear the smoothing history");
}
int main(){try{
    controlRanges();sensitivityTrim();smoothingControl();
    demoResponse(1);demoResponse(.015f);
    AudioRing ring;Analyzer analyzer;std::array<float,512> samples{};
    for(int frame=0;frame<40;++frame){ring.push(samples.data(),256);analyzer.update(ring,256/48000.f);}
    float latency=-1;float peak=0;
    for(int frame=0;frame<100;++frame){
        for(int i=0;i<256;++i){float t=float(frame*256+i)/48000;float s=.03f*std::sin(6.2831853f*80*t)*std::exp(-t*40);samples[i*2]=s;samples[i*2+1]=-s;}
        ring.push(samples.data(),256);analyzer.update(ring,256/48000.f);
        if(latency<0&&analyzer.value.beat>.3f)latency=(frame+1)*256/48.f;
        peak=std::max(peak,analyzer.value.energy);
    }
    std::cout<<"Transient latency="<<latency<<" ms; peak="<<peak<<"; released="<<analyzer.value.energy<<"\n";
    require(latency>0&&latency<=60,"A transient must trigger within 60 ms of delivered samples");
    require(peak>.5f&&analyzer.value.energy<.04f,"An attack must rise strongly and release promptly");
    analyzer.reset();uint64_t cursor=0;int falseOnsets=0;
    for(int frame=0;frame<180;++frame){
        std::array<float,1600> tone{};
        for(int i=0;i<800;++i){float v=.002f*std::sin(6.2831853f*1000*float(cursor++)/48000);tone[i*2]=tone[i*2+1]=v;}
        ring.push(tone.data(),800);analyzer.update(ring,1/60.f);
        if(frame>60&&analyzer.value.beat>.2f)++falseOnsets;
    }
    require(falseOnsets==0,"A steady quiet tone must not create fake rhythmic onsets");
    require(analyzer.value.energy>.3f,"Quiet sustained audio must remain visible with automatic level tracking");
    require(soundMotion(analyzer.value,0)==1,"Zero reactivity must preserve unmodulated motion");
    analyzer.reset();for(int i=0;i<120;++i)analyzer.update(ring,1/60.f);
    require(analyzer.value.energy==0&&analyzer.value.beat==0,"Silence must not manufacture motion cues");
    std::cout<<"PASS: control ranges, low-end sensitivity, smoothing, demo dynamics, quiet demo, transient latency/release, sustained audio, silence and zero response.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
