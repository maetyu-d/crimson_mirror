#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <complex>
#include <cmath>
#include <cstdint>

namespace prism {
struct StereoFrame { float l=0,r=0; };
class AudioRing {
    static constexpr uint32_t capacity=65536;
    std::array<StereoFrame,capacity> frames{};
    alignas(64) std::atomic<uint32_t> write{0};
    alignas(64) std::atomic<uint32_t> read{0};
public:
    // One audio producer and one graphics consumer. Overflow drops new frames.
    void push(const float* input,uint32_t count,int channels=2) {
        auto w=write.load(std::memory_order_relaxed);
        const auto r=read.load(std::memory_order_acquire);
        count=std::min(count,capacity-(w-r));
        for(uint32_t i=0;i<count;++i) frames[(w+i)%capacity]={input[i*channels],input[i*channels+(channels>1?1:0)]};
        write.store(w+count,std::memory_order_release);
    }
    template<class F> uint32_t drain(F&& consume) {
        auto r=read.load(std::memory_order_relaxed);
        const auto w=write.load(std::memory_order_acquire);
        const uint32_t n=w-r;
        while(r!=w) consume(frames[r++%capacity]);
        read.store(r,std::memory_order_release); return n;
    }
};
struct Features {
    std::array<float,28> bands{};
    float bass=0,mid=0,high=0,beat=0,rms=0,peak=0,energy=0;
    float dominantHz=0;
};
class Analyzer {
    static constexpr int fftSize=2048;
    std::array<StereoFrame,fftSize> history{};
    std::array<std::complex<float>,fftSize> left{},right{};
    std::array<float,fftSize/2> magnitudes{},previous{};
    int cursor=0;
    float silentTime=0,averageFlux=.02f,beatHold=0,referenceRms=.02f,beatEnvelope=0;
    bool levelReady=false;
    const std::array<float,fftSize> window=[] {
        std::array<float,fftSize> result{};
        for(int i=0;i<fftSize;++i)result[i]=.5f-.5f*std::cos(6.28318530718f*i/(fftSize-1));
        return result;
    }();
    static void fft(std::array<std::complex<float>,fftSize>& a) {
        for(int i=1,j=0;i<fftSize;++i) {
            int bit=fftSize>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit;
            if(i<j) std::swap(a[i],a[j]);
        }
        for(int length=2;length<=fftSize;length<<=1) {
            const float angle=-6.28318530718f/length;
            const std::complex<float> delta(std::cos(angle),std::sin(angle));
            for(int i=0;i<fftSize;i+=length) {
                std::complex<float> w(1,0);
                for(int j=0;j<length/2;++j) {
                    auto u=a[i+j],v=a[i+j+length/2]*w;
                    a[i+j]=u+v; a[i+j+length/2]=u-v; w*=delta;
                }
            }
        }
    }
    static float smooth(float from,float to,float dt) {
        return from+(to-from)*(1-std::exp(-dt/(to>from?.010f:.105f)));
    }
public:
    Features value;
    // A main-thread view of the same samples used by the FFT, never a second
    // audio consumer. Keep stereo separate so a phase-inverted signal is visible.
    std::array<StereoFrame,128> scope() const {
        std::array<StereoFrame,128> result{};
        if(silentTime>.045f)return result;
        for(size_t i=0;i<result.size();++i)result[i]=history[(cursor+int(i)*fftSize/int(result.size()))%fftSize];
        return result;
    }
    void reset() {
        history.fill({}); previous.fill(0); cursor=0; silentTime=0;
        averageFlux=.02f; beatHold=0; referenceRms=.02f;beatEnvelope=0;levelReady=false;value={};
    }
    void update(AudioRing& ring,float dt,float gain=1,float sampleRate=48000) {
        dt=std::clamp(dt,.0001f,.25f);
        const auto received=ring.drain([&](StereoFrame f){history[cursor]=f;cursor=(cursor+1)%fftSize;});
        silentTime=received?0:silentTime+dt;
        float rms=0,peak=0,shortPower=0;
        for(int i=0;i<fftSize;++i) {
            auto f=silentTime>.045f?StereoFrame{}:history[(cursor+i)%fftSize];
            left[i]={f.l*window[i],0}; right[i]={f.r*window[i],0};
            float power=(f.l*f.l+f.r*f.r)*.5f;rms+=power;
            if(i>=fftSize-512)shortPower+=power;
            peak=std::max(peak,std::max(std::abs(f.l),std::abs(f.r)));
        }
        value.rms=smooth(value.rms,std::sqrt(rms/fftSize),dt);
        value.peak=std::max(peak,value.peak*std::exp(-dt*6));
        const float fastRms=std::sqrt(shortPower/512.f);
        if(!levelReady&&fastRms>.0005f){referenceRms=std::max(.0015f,fastRms);levelReady=true;}
        // A slow reference keeps quiet tracks useful without flattening each
        // drum hit. The short envelope catches attacks before the FFT settles.
        const float adapt=fastRms>referenceRms?.18f:2.2f;
        referenceRms+=(fastRms-referenceRms)*(1-std::exp(-dt/adapt));
        const float reference=std::max(.0015f,referenceRms)*1.6f;
        const float gate=std::clamp((fastRms-.00015f)/.0005f,0.f,1.f);
        auto response=[&](float amplitude){return std::clamp(amplitude*gain/reference,0.f,1.f)*gate;};
        value.energy=smooth(value.energy,response(fastRms),dt);
        fft(left); fft(right);
        float flux=0,maxMagnitude=0; int dominant=0;
        for(int i=1;i<fftSize/2;++i) {
            // Average channel powers, preserving out-of-phase stereo signals.
            float m=std::sqrt((std::norm(left[i])+std::norm(right[i]))*.5f)*4.f/fftSize;
            magnitudes[i]=m; flux+=std::max(0.f,m-previous[i]); previous[i]=m;
            if(m>maxMagnitude) {maxMagnitude=m;dominant=i;}
        }
        value.dominantHz=maxMagnitude>1e-5f?dominant*sampleRate/fftSize:0;
        beatHold=std::max(0.f,beatHold-dt);
        beatEnvelope*=std::exp(-dt*12);
        const float relativeFlux=flux/std::max(.004f,referenceRms*4);
        const float threshold=averageFlux*1.45f+.025f;
        if(received&&relativeFlux>threshold&&fastRms>.0005f&&beatHold<=0) {
            beatEnvelope=std::clamp((relativeFlux-threshold)/std::max(.08f,averageFlux*.6f),.35f,1.f);
            beatHold=.115f;
        }
        // Manual sensitivity trims transients as well as sustained energy;
        // automatic level tracking must not defeat attenuation near zero.
        value.beat=std::clamp(beatEnvelope*gain,0.f,1.f);
        averageFlux+=(relativeFlux-averageFlux)*(1-std::exp(-dt*3));
        auto level=[&](float lo,float hi) {
            int start=std::clamp(int(std::floor(lo*fftSize/sampleRate)),1,fftSize/2-1);
            int end=std::clamp(int(std::ceil(hi*fftSize/sampleRate)),start+1,fftSize/2);
            float power=0; for(int i=start;i<end;++i) power+=magnitudes[i]*magnitudes[i];
            // Hann-normalised band RMS; linear amplitude retains the contrast
            // that a wide dB scale previously compressed into a steady level.
            return response(std::sqrt(power/3.f));
        };
        for(int band=0;band<28;++band) {
            float lo=30.f*std::pow(16000.f/30.f,band/28.f);
            float hi=30.f*std::pow(16000.f/30.f,(band+1)/28.f);
            value.bands[band]=smooth(value.bands[band],level(lo,hi),dt);
        }
        value.bass=smooth(value.bass,level(30,220),dt);
        value.mid=smooth(value.mid,level(220,2400),dt);
        value.high=smooth(value.high,level(2400,16000),dt);
    }
};
}
