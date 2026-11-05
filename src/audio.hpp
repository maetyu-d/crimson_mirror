#pragma once
#include "analysis.hpp"
#include <memory>
#include <string>
#include <vector>

namespace prism {
enum class AudioSource { None,File,Input,Demo };
struct InputDevice { std::string name; int index; };
class Audio {
    struct State;
    std::unique_ptr<State> state;
public:
    AudioRing ring;
    std::string title="Choose a track or live input",error;
    AudioSource source=AudioSource::None;
    Audio(); ~Audio();
    Audio(const Audio&)=delete; Audio& operator=(const Audio&)=delete;
    bool open(const std::string& path);
    bool demo();
    std::vector<InputDevice> inputs();
    bool input(int index=-1);
    void stop();
    bool playing() const;
    void setPlaying(bool value);
    void setVolume(float value);
    double position() const;
    double duration() const;
    void seek(double seconds);
};
void synthesize(float* stereo,uint32_t count,uint64_t start);
std::string chooseAudioFile();
std::string choosePngFile(const std::string& suggested);
bool microphoneAccess();
}
