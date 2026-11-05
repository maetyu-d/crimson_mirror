#pragma once
#include "raylib.h"
#include <array>
namespace prism {
struct SceneBindings {
    bool visible=false;
    std::array<Vector2,2> roots{}; // mesh, material: node ports in logical pixels
};
}
