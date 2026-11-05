#pragma once
#include "analysis.hpp"
namespace prism {
struct ControlRange {
    float minimum,maximum,curve;
    float fromPosition(float position) const {
        return minimum+(maximum-minimum)*std::pow(std::clamp(position,0.f,1.f),curve);
    }
    float toPosition(float value) const {
        return std::pow(std::clamp((value-minimum)/(maximum-minimum),0.f,1.f),1.f/curve);
    }
};
inline constexpr ControlRange sensitivityRange{0,16,3};
inline constexpr ControlRange reactivityRange{0,8,3};
inline constexpr ControlRange smoothingRange{0,2000,2};

class FeatureSmoother {
    Features state;
public:
    void reset(){state={};}
    const Features& update(const Features& input,float dt,float milliseconds) {
        if(milliseconds<=0){state=input;return state;}
        const float alpha=-std::expm1(-std::clamp(dt,0.f,.25f)/(std::clamp(milliseconds,0.f,2000.f)*.001f));
        auto follow=[&](float& value,float target){value+=(target-value)*alpha;};
        for(size_t i=0;i<state.bands.size();++i)follow(state.bands[i],input.bands[i]);
        follow(state.bass,input.bass);follow(state.mid,input.mid);follow(state.high,input.high);
        follow(state.energy,input.energy);follow(state.beat,input.beat);
        // Keep the actual input meters truthful while smoothing visual motion.
        state.rms=input.rms;state.peak=input.peak;state.dominantHz=input.dominantHz;
        return state;
    }
};
inline float soundMotion(const Features& audio,float strength) {
    const float activity=.12f+audio.energy*1.9f+audio.beat*.9f;
    return std::clamp(1.f+std::clamp(strength,reactivityRange.minimum,reactivityRange.maximum)*(activity-1.f),.12f,4.f);
}
}
