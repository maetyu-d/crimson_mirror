#include "audio.hpp"
#include "webcam.hpp"
namespace prism {
struct Webcam::State { bool enabled=false; };
Webcam::Webcam():state(std::make_unique<State>()){}
Webcam::~Webcam()=default;
void Webcam::setEnabled(bool enabled){state->enabled=enabled;}
bool Webcam::poll(WebcamFrame&,std::string& error){
    if(state->enabled)error="Webcam texture is available on macOS.";
    return false;
}
std::string chooseAudioFile(){return {};}
std::string choosePngFile(const std::string& suggested){return suggested;}
bool microphoneAccess(){return true;}
}
