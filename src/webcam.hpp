#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace prism {
struct WebcamFrame {
    int width=0,height=0;
    std::vector<uint8_t> bgra; // packed rows, top first
};

class Webcam {
    struct State;
    std::unique_ptr<State> state;
public:
    Webcam();
    ~Webcam();
    void setEnabled(bool enabled);
    // Consume the newest frame only. A capture failure is reported in error.
    bool poll(WebcamFrame& frame,std::string& error);
};
}
