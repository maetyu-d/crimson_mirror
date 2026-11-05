#include "evolution.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace prism;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::array<float,5> weights(const EvolvingForm& f){
    std::array<float,5> w{};w[f.look.shape]+=1-f.look.shapeBlend;w[f.look.nextShape]+=f.look.shapeBlend;return w;
}
int main(){try{
    Look base;const auto start=FormEvolution{}.sample(base);
    require(start.look.shape==base.shape&&start.look.shapeBlend==0&&start.breath==1&&start.fieldScale==1,"Evolution must start at the selected base form");
    FormEvolution clock;
    clock.update(3,true,false);clock.update(100,true,true);clock.update(100,false,false);
    require(clock.sample(base).seconds==3,"Freeze and disabling evolution must hold its clock");
    FormEvolution fast,slow;
    for(int i=0;i<600;++i)fast.update(1/60.,true,false);
    for(int i=0;i<2400;++i)slow.update(1/240.,true,false);
    require(std::abs(fast.sample(base).seconds-slow.sample(base).seconds)<1e-9,"Evolution clock must be independent of frame rate");
    const auto a=FormEvolution(6).sample(base),b=FormEvolution(18).sample(base);
    require(a.look.shapeBlend>.4f&&a.look.shapeBlend<.6f&&b.look.shape!=a.look.shape,"Elapsed time must alter the actual form");
    require(std::abs(a.look.flowScale-base.flowScale)>.01f&&std::abs(a.look.size-base.size)>.01f,"Slow cycles must alter flow and prism profile as well as the boundary");
    for(int shape=0;shape<5;++shape){
        base.shape=shape;
        for(int boundary=12;boundary<=120;boundary+=12){
            const auto before=weights(FormEvolution(boundary-.001).sample(base));
            const auto after=weights(FormEvolution(boundary+.001).sample(base));
            for(int k=0;k<5;++k)require(std::abs(before[k]-after[k])<1e-5f,"Shape weights must remain continuous at every keyframe and loop wrap");
        }
        for(int t=0;t<=6000;++t){
            const auto f=FormEvolution(t*.1).sample(base);
            require(f.look.shape>=0&&f.look.shape<=4&&f.look.nextShape>=0&&f.look.nextShape<=4,"All base shapes must cycle through valid containers");
            require(std::isfinite(f.look.shapeBlend)&&f.look.shapeBlend>=0&&f.look.shapeBlend<=1,"Morph weights must stay bounded and finite");
            require(f.breath>=.889f&&f.breath<=1.111f&&f.fieldScale>=.739f&&f.fieldScale<=1.261f,"Long-running modulation must stay within its intended limits");
            require(f.look.size>0&&f.look.shapeSize>0&&f.look.flowScale>0,"Evolving geometry parameters must stay positive");
        }
        const auto fixed=FormEvolution(24).sample(base,false);
        require(fixed.look.shape==shape&&fixed.look.shapeBlend==0&&fixed.look.shapeSize==base.shapeSize&&fixed.look.stretch==base.stretch&&fixed.look.flowScale==base.flowScale,"Disabling evolution must restore the selected base parameters");
    }
    require(FormEvolution(std::nan("")).sample(base).seconds==0,"Invalid initial time must not reach the GPU");
    std::cout<<"PASS: independent clock, freeze/bypass, frame-rate independence, evolving geometry, continuous shape weights, all base forms and bounded long-running modulation.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
