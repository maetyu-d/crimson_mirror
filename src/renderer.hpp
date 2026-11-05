#pragma once
#include "analysis.hpp"
#include "look.hpp"
#include "bindings.hpp"
#include "raylib.h"
#include <memory>
#include <string>
namespace prism {
struct WebcamFrame;
class Renderer {
    struct State;
    std::unique_ptr<State> state;
public:
    Camera3D camera{{0,0,58},{0,0,0},{0,1,0},35,CAMERA_PERSPECTIVE};
    float reactivity=1;
    float afterimage=1.25f; // history decay time constant, seconds; zero bypasses
    explicit Renderer(const std::string& assets);
    ~Renderer();
    void reset();
    void clearFeedback();
    void uploadWebcamFrame(const WebcamFrame&);
    void clearWebcam();
    Texture2D webcamPreview() const;
    void palette(int scheme,uint32_t seed,int slot=0);
    void useSlot(int slot);
    void step(float dt,const Look&,const Features&);
    void draw(const Look&,const Features&,int width,int height,const SceneBindings& bindings={},float frameDt=0);
    float simulationTime() const;
    std::array<Color,20> palettePreview() const;
    void finish();
    bool validate(std::string& report);
};
}
