#pragma once
#include "audio.hpp"
#include "look.hpp"
#include "evolution.hpp"
#include "bindings.hpp"
#include "raylib.h"
#include <array>

namespace prism {
// A fictional patch notation over the real, fixed C++ signal path. Values are
// borrowed from the running analyser/renderer; moving a node only moves its UI.
class PatchView {
    struct Node { Vector2 position; float height; const char* title; };
    std::array<Node,16> nodes;
    std::array<float,128> energy{},onsets{};
    float historyTime=0,scale=1;
    Vector2 origin{},dragOffset{};
    int historyCursor=0,dragging=-1,selected=-1,hovered=-1;
    Rectangle bounds(int index) const;
public:
    PatchView();
    void resetLayout();
    void update(const Features& analysed,const Features& smoothed,float dt);
    // Return true while the patch owns the pointer, so node drags never orbit.
    bool interact(int width,int height,bool enabled,bool visible);
    SceneBindings sceneBindings(bool visible) const;
    void draw(Font,const Analyzer&,const Features&,const EvolvingForm&,
              float sensitivity,float reactivity,float smoothing,float simulationTime,bool frozen,float afterimage,
              const std::array<Color,20>& palette,Texture2D webcam,int width,int height) const;
};
}
