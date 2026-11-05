#include "analysis.hpp"
#include "audio.hpp"
#include "look.hpp"
#include "miniaudio.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace prism;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
Features tone(float hz,float sampleRate=48000,bool inverse=false,float gain=1) {
    AudioRing ring;Analyzer analyzer;std::vector<float> data(1600);uint64_t frame=0;
    for(int step=0;step<90;++step){
        for(int i=0;i<800;++i){float v=.2f*std::sin(6.28318530718f*hz*float(frame++)/sampleRate);data[i*2]=v;data[i*2+1]=inverse?-v:v;}
        ring.push(data.data(),800);analyzer.update(ring,800/sampleRate,gain,sampleRate);
    }
    return analyzer.value;
}
void writeWav(const std::filesystem::path& path){
    std::ofstream f(path,std::ios::binary);auto w16=[&](uint16_t v){f.put(char(v));f.put(char(v>>8));};
    auto w32=[&](uint32_t v){w16(uint16_t(v));w16(uint16_t(v>>16));};
    f.write("RIFF",4);w32(36+48000*4);f.write("WAVEfmt ",8);w32(16);w16(1);w16(2);w32(48000);w32(48000*4);w16(4);w16(16);f.write("data",4);w32(48000*4);
    for(int i=0;i<48000;++i){int16_t value=int16_t(12000*std::sin(6.28318530718*440*i/48000));w16(uint16_t(value));w16(uint16_t(-value));}
}
int main(int argc,char** argv){try{
    AudioRing ring;Analyzer analyzer;analyzer.update(ring,1/60.f);
    require(analyzer.value.rms==0&&analyzer.value.bass==0&&analyzer.value.beat==0,"Silence must produce zero audio features");
    auto low=tone(82),mid=tone(1000),high=tone(6500);
    require(low.bass>low.high+.3f,"Bass tone must energise the bass region");
    require(mid.mid>mid.bass+.3f&&mid.mid>mid.high+.2f,"Mid tone must energise the mid region");
    require(high.high>high.bass+.3f,"Treble tone must energise the treble region");
    require(std::abs(mid.dominantHz-1000)<14,"FFT must find the 1 kHz peak at 48 kHz");
    auto at441=tone(1000,44100);require(std::abs(at441.dominantHz-1000)<12,"FFT must use the actual sample rate");
    auto inverted=tone(82,48000,true);require(std::abs(inverted.bass-low.bass)<.001f,"Out-of-phase stereo must not cancel the visualiser");
    require(std::abs(low.rms-.141421f)<.003f,"RMS calibration must match a 0.2-amplitude sine");
    auto louder=tone(1000,48000,false,2);require(louder.mid>=mid.mid,"Sensitivity gain must not reduce response");
    std::vector<float> data(1600);synthesize(data.data(),800,0);ring.push(data.data(),800);analyzer.update(ring,1/60.f);
    for(int i=0;i<240;++i)analyzer.update(ring,1/60.f);
    require(analyzer.value.rms<1e-5f&&analyzer.value.mid<1e-5f,"Stopping input must release meters to silence");
    for(auto sample:analyzer.scope())require(sample.l==0&&sample.r==0,"Patch scope must clear after input stops");
    // The patch observes the existing analysis buffer without draining samples
    // from the audio queue or folding opposite-polarity stereo to mono.
    std::vector<float> stereo(2048*2);
    for(int i=0;i<2048;++i){stereo[i*2]=.2f;stereo[i*2+1]=-.1f;}
    ring.push(stereo.data(),2048);analyzer.update(ring,1/60.f);
    for(auto sample:analyzer.scope())require(sample.l==.2f&&sample.r==-.1f,"Patch scope must preserve actual left/right samples");
    analyzer.reset();
    for(auto sample:analyzer.scope())require(sample.l==0&&sample.r==0,"Changing sources must clear the patch scope");
    // Saturate the SPSC queue. New frames are dropped instead of overwriting unread data.
    std::vector<float> saturated(70000*2,.3f);ring.push(saturated.data(),70000);uint32_t count=0;
    ring.drain([&](StereoFrame frame){++count;require(frame.l==.3f&&frame.r==.3f,"Ring data must be intact");});
    require(count==65536,"Ring capacity and overflow policy must be bounded");
    auto fixture=std::filesystem::temp_directory_path()/"prism-currents-test.wav";writeWav(fixture);
    ma_decoder decoder{};auto config=ma_decoder_config_init(ma_format_f32,2,48000);
    require(ma_decoder_init_file(fixture.c_str(),&config,&decoder)==MA_SUCCESS,"WAV decoder must open stereo PCM");
    ma_uint64 length=0;ma_decoder_get_length_in_pcm_frames(&decoder,&length);require(length==48000,"Decoder duration must be sample accurate");
    ma_decoder_seek_to_pcm_frame(&decoder,24000);ma_uint64 got=0;std::vector<float> decoded(4096*2);
    ma_decoder_read_pcm_frames(&decoder,decoded.data(),4096,&got);require(got==4096,"Decoder must support seeking and reading");
    float max=0;for(size_t i=0;i<got;++i){max=std::max(max,std::abs(decoded[i*2]));require(std::abs(decoded[i*2]+decoded[i*2+1])<1e-6f,"Stereo channel polarity must be preserved by decoding");}
    require(max>.3f,"Decoder must output audible samples");ma_decoder_uninit(&decoder);
    for(int i=1;i<argc;++i){
        ma_decoder external{};
        require(ma_decoder_init_file(argv[i],&config,&external)==MA_SUCCESS,"Additional format must decode");
        ma_decoder_get_length_in_pcm_frames(&external,&length);
        require(length>40000&&length<60000,"Additional fixture duration must be close to one second");
        ma_decoder_read_pcm_frames(&external,decoded.data(),4096,&got);
        float power=0;for(size_t j=0;j<got*2;++j)power+=decoded[j]*decoded[j];
        require(got==4096&&power>1,"Additional format must produce actual audio samples");
        ma_decoder_uninit(&external);std::cout<<"Decoded: "<<argv[i]<<"\n";
    }
    for(size_t i=0;i<schemes.size();++i){auto palette=rollPalette(int(i),161301);for(auto& p:palette)require(std::isfinite(p.coeffs.a[0])&&p.strength>=0,"Every source palette must be usable");}
    require(originalPresets().size()==16,"All sixteen original presets must be present");
    std::cout<<"PASS: silence, FFT frequency regions, 44.1/48 kHz, stereo polarity, RMS, sensitivity, release, stereo patch scope, queue overflow, WAV decode/seek and all original palettes/presets.\n";
    std::cout<<"Playback fixture: "<<fixture<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
