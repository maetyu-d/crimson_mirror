#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.hpp"
#include <algorithm>
#include <filesystem>

namespace prism {
void synthesize(float* output,uint32_t frames,uint64_t start) {
    constexpr double pi=3.14159265358979323846;
    constexpr double chords[4][3]={{130.8128,164.8138,195.9977},{110,130.8128,164.8138},
        {87.3071,130.8128,174.6141},{97.9989,146.8324,195.9977}};
    for(uint32_t i=0;i<frames;++i) {
        const double t=double(start+i)/48000.;
        const double beat=t*112./60.,phase=beat-std::floor(beat);
        int chord=int(beat/8)%4;
        double pad=0; for(double hz:chords[chord]) pad+=std::sin(2*pi*hz*t)+.2*std::sin(2*pi*hz*2.003*t);
        double kick=std::sin(2*pi*(49*t+2.4*(1-std::exp(-phase*36))))*std::exp(-phase*17);
        double arp=std::sin(2*pi*chords[chord][int(beat*2)%3]*4*t)*std::exp(-(beat*2-std::floor(beat*2))*8);
        double h=std::sin(2*pi*7471*t)*std::sin(2*pi*9137*t)*std::exp(-(beat*4-std::floor(beat*4))*26);
        const float common=float(pad*.035+kick*.22+arp*.06+h*.035);
        output[i*2]=common;
        output[i*2+1]=float(common*.92+std::sin(2*pi*chords[chord][2]*1.001*t)*.025);
    }
}
struct Audio::State {
    Audio* owner;
    ma_context context{};
    ma_device device{};
    ma_decoder decoder{};
    bool contextReady=false,deviceReady=false,decoderReady=false;
    std::atomic<bool> running{false};
    std::atomic<float> volume{.65f};
    std::atomic<uint64_t> frame{0};
    std::atomic<int> mode{0};
    uint64_t length=0;
    std::vector<ma_device_id> captureIds;
    explicit State(Audio* p):owner(p){}
    static void callback(ma_device* device,void* output,const void* input,ma_uint32 count) {
        auto& s=*static_cast<State*>(device->pUserData);
        if(device->type==ma_device_type_capture) {
            if(input&&s.running.load(std::memory_order_relaxed)) s.owner->ring.push(static_cast<const float*>(input),count);
            return;
        }
        auto* out=static_cast<float*>(output);
        std::fill(out,out+count*2,0.f);
        if(!s.running.load(std::memory_order_relaxed)) return;
        uint64_t got=count;
        if(s.mode.load(std::memory_order_relaxed)==1) {
            ma_uint64 decoded=0;
            ma_decoder_read_pcm_frames(&s.decoder,out,count,&decoded);got=decoded;
            if(decoded<count) s.running.store(false,std::memory_order_relaxed);
        } else synthesize(out,count,s.frame.load(std::memory_order_relaxed));
        s.frame.fetch_add(got,std::memory_order_relaxed);
        // Analysis is pre-fader: muting the speakers keeps the visualiser active.
        s.owner->ring.push(out,count);
        const float volume=s.volume.load(std::memory_order_relaxed);
        for(uint32_t i=0;i<count*2;++i) out[i]*=volume;
    }
    bool contextInit() {
        if(!contextReady) {
            auto result=ma_context_init(nullptr,0,nullptr,&context);
            contextReady=result==MA_SUCCESS;
            if(!contextReady) owner->error="Audio unavailable: "+std::string(ma_result_description(result));
        }
        return contextReady;
    }
    bool start(ma_device_config& config) {
        if(!contextInit()) return false;
        config.sampleRate=48000; config.periodSizeInFrames=512;
        config.dataCallback=callback;config.pUserData=this;
        auto result=ma_device_init(&context,&config,&device);
        if(result!=MA_SUCCESS) {owner->error="Cannot open device: "+std::string(ma_result_description(result));return false;}
        deviceReady=true;running.store(true);
        result=ma_device_start(&device);
        if(result!=MA_SUCCESS) {owner->error="Cannot start device: "+std::string(ma_result_description(result));running.store(false);ma_device_uninit(&device);deviceReady=false;return false;}
        return true;
    }
};
Audio::Audio():state(std::make_unique<State>(this)){}
Audio::~Audio(){stop();if(state->contextReady)ma_context_uninit(&state->context);}
void Audio::stop() {
    state->running.store(false);
    if(state->deviceReady) {ma_device_uninit(&state->device);state->deviceReady=false;}
    if(state->decoderReady) {ma_decoder_uninit(&state->decoder);state->decoderReady=false;}
    state->frame=0;state->length=0;state->mode=0;
    ring.drain([](StereoFrame){});source=AudioSource::None;
}
bool Audio::open(const std::string& path) {
    stop();error.clear();
    auto config=ma_decoder_config_init(ma_format_f32,2,48000);
    auto result=ma_decoder_init_file(path.c_str(),&config,&state->decoder);
    if(result!=MA_SUCCESS) {error="Cannot decode this file. Choose WAV, MP3 or FLAC.";return false;}
    state->decoderReady=true;
    ma_decoder_get_length_in_pcm_frames(&state->decoder,&state->length);
    state->mode=1;
    auto device=ma_device_config_init(ma_device_type_playback);
    device.playback.format=ma_format_f32;device.playback.channels=2;
    if(!state->start(device)) {stop();return false;}
    source=AudioSource::File;title=std::filesystem::path(path).filename().string();return true;
}
bool Audio::demo() {
    stop();error.clear();state->mode=2;
    auto device=ma_device_config_init(ma_device_type_playback);
    device.playback.format=ma_format_f32;device.playback.channels=2;
    if(!state->start(device)) {stop();return false;}
    source=AudioSource::Demo;title="Prism study / 112 BPM";return true;
}
std::vector<InputDevice> Audio::inputs() {
    std::vector<InputDevice> result;state->captureIds.clear();
    if(!state->contextInit()) return result;
    ma_device_info* captures=nullptr;ma_uint32 count=0;
    if(ma_context_get_devices(&state->context,nullptr,nullptr,&captures,&count)!=MA_SUCCESS) return result;
    for(ma_uint32 i=0;i<count;++i) {result.push_back({captures[i].name,int(i)});state->captureIds.push_back(captures[i].id);}
    return result;
}
bool Audio::input(int index) {
    if(!microphoneAccess()) {error="Microphone access is off. Enable Prism Currents in System Settings > Privacy & Security > Microphone.";return false;}
    stop();error.clear();
    auto config=ma_device_config_init(ma_device_type_capture);
    config.capture.format=ma_format_f32;config.capture.channels=2;
    if(index>=0&&index<int(state->captureIds.size())) config.capture.pDeviceID=&state->captureIds[index];
    if(!state->start(config)) return false;
    source=AudioSource::Input;title=state->device.capture.name;return true;
}
bool Audio::playing()const{return state->running.load();}
void Audio::setPlaying(bool value) {
    if(value&&source==AudioSource::File&&state->frame>=state->length) seek(0);
    if(state->deviceReady) state->running.store(value);
}
void Audio::setVolume(float value){state->volume=std::clamp(value,0.f,1.f);}
double Audio::position()const{return double(state->frame.load())/48000.;}
double Audio::duration()const{return double(state->length)/48000.;}
void Audio::seek(double seconds) {
    if(!state->decoderReady||!state->deviceReady)return;
    bool wasPlaying=playing();
    ma_device_stop(&state->device); // wait until the callback releases the decoder
    auto frame=uint64_t(std::clamp(seconds,0.,duration())*48000.);
    ma_decoder_seek_to_pcm_frame(&state->decoder,frame);state->frame=frame;
    ring.drain([](StereoFrame){});state->running=wasPlaying;
    ma_device_start(&state->device);
}
}
