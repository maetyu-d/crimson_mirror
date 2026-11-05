#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace prism {
constexpr int routeCount = 28;
constexpr int maxParticles = 100000;
constexpr int crimsonScheme = 18;
struct Look {
    float speed=1, flowScale=.085f, flowSpeed=3.2f, evolve=.035f, drift=.5f, steer=4;
    float spawnRadius=1.8f, lifeMin=6, lifeMax=14, size=.3f, stretch=1, spread=.35f;
    float shimmer=.015f, gloss=1, uniformity=0, nearFade=0, fov=35;
    float lines=.55f, ao=1, aoRadius=.9f, fog=140, shapeSize=12, cling=0;
    float paletteScale=1, paletteOffset=0, unify=0, focus=0, aperture=4, maxBlur=10;
    float switchTime=20;
    float shapeBlend=0;
    int count=40000, shape=0, nextShape=0;
    bool dark=true, dof=false, secondPalette=false;
};
struct Preset {
    std::string name;
    Look look;
    int scheme=0, scheme2=13;
    uint32_t seed=161301, seed2=959004;
    std::array<float,3> position={0,0,58}, target={0,0,0};
};
using Triple = std::array<float,3>;
struct Coefficients { Triple a,b,c,d; };
struct Range { float low,high; };
struct Weighted { int id; float weight; };
struct PaletteSpec {
    std::vector<Weighted> choices;
    Range length,offset,strength,ambience;
};
struct Scheme { std::string name; PaletteSpec bg,tracks; bool lockStrength,lockAmbience; };
struct PaletteRow { Coefficients coeffs; float length,offset,strength,ambience; };
#include "reference_data.hpp"

// The original mulberry32 generator keeps scheme/seed rolls reproducible.
class Random {
    uint32_t state;
public:
    explicit Random(uint32_t seed):state(seed){}
    float next() {
        uint32_t t=(state+=0x6d2b79f5u);
        t=(t^(t>>15))*(t|1u);
        t^=t+((t^(t>>7))*(t|61u));
        return float(double(t^(t>>14))/4294967296.0);
    }
    float range(Range r) { return r.low+(r.high-r.low)*next(); }
};
inline std::array<PaletteRow,29> rollPalette(int index,uint32_t seed) {
    const auto& scheme=schemes.at(index);
    Random random(seed);
    const float strength=scheme.lockStrength?random.range(scheme.bg.strength):0;
    const float ambience=scheme.lockAmbience?random.range(scheme.bg.ambience):0;
    std::array<PaletteRow,29> rows;
    for(int i=0;i<29;++i) {
        const auto& spec=i==0?scheme.bg:scheme.tracks;
        float total=0; for(auto c:spec.choices) total+=c.weight;
        float r=random.next()*total; int id=spec.choices.back().id;
        for(auto c:spec.choices) if((r-=c.weight)<=0) { id=c.id; break; }
        int paletteIndex=id==19?17:(id>=1&&id<=17?id-1:13);
        rows[i]={iqPalettes[paletteIndex],random.range(spec.length),random.range(spec.offset),
            scheme.lockStrength?strength:random.range(spec.strength),
            scheme.lockAmbience?ambience:random.range(spec.ambience)};
    }
    return rows;
}
}
