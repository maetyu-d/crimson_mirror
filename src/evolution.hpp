#pragma once
#include "look.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace prism {
inline constexpr double formCycleSeconds=60;
inline constexpr double breathCycleSeconds=18;
inline const char* shapeName(int shape){
    constexpr const char* names[]={"slab","sphere","torus","capsule","wave"};
    return names[std::clamp(shape,0,4)];
}
struct EvolvingForm {
    Look look;
    double seconds=0;
    int baseShape=0;
    float baseSize=12,phase=0,breath=1,fieldScale=1,profile=1;
    bool enabled=true;
};
class FormEvolution {
    double elapsed=0;
public:
    explicit FormEvolution(double start=0):elapsed(std::isfinite(start)?std::max(0.,start):0){}
    void update(double dt,bool enabled,bool frozen){
        if(enabled&&!frozen&&std::isfinite(dt)&&dt>0)elapsed+=dt;
    }
    EvolvingForm sample(const Look& base,bool enabled=true) const {
        EvolvingForm form;form.look=base;form.seconds=elapsed;form.enabled=enabled;
        form.baseShape=std::clamp(base.shape,0,4);form.baseSize=base.shapeSize;
        form.look.nextShape=form.look.shape=form.baseShape;form.look.shapeBlend=0;
        if(!enabled)return form;
        // Rotate a fixed sequence to start with the user's selected container.
        // Smoothstep gives zero slope at both ends of every shape transition.
        constexpr std::array<int,5> sequence{0,2,1,3,4};
        const int offset=int(std::find(sequence.begin(),sequence.end(),form.baseShape)-sequence.begin());
        form.phase=float(std::fmod(elapsed,formCycleSeconds)/formCycleSeconds);
        const float stage=form.phase*sequence.size();const int index=int(stage);
        const float t=stage-index;
        form.look.shape=sequence[(offset+index)%sequence.size()];
        form.look.nextShape=sequence[(offset+index+1)%sequence.size()];
        form.look.shapeBlend=t*t*(3-2*t);
        constexpr double tau=6.283185307179586;
        form.breath=1+.11f*float(std::sin(tau*elapsed/breathCycleSeconds));
        form.fieldScale=1+.26f*float(std::sin(tau*elapsed/37));
        form.profile=1+.24f*float(std::sin(tau*elapsed/29));
        form.look.shapeSize=base.shapeSize*form.breath;
        form.look.flowScale=base.flowScale*form.fieldScale;
        form.look.size=base.size*(1+(form.breath-1)*.55f);
        form.look.stretch=base.stretch*form.profile;
        return form;
    }
};
}
