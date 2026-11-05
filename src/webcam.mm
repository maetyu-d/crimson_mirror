#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include "webcam.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>

namespace {
using Clock=std::chrono::steady_clock;
dispatch_queue_t cameraQueue() {
    static dispatch_queue_t queue=dispatch_queue_create("studio.prismcurrents.webcam",DISPATCH_QUEUE_SERIAL);
    return queue;
}
}

@interface PrismWebcamCapture : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
@public
    std::atomic<bool> cancelled;
    std::mutex mutex;
    prism::WebcamFrame latest;
    bool frameReady;
    std::string error;
    Clock::time_point lastFrame;
    AVCaptureSession* session;
    AVCaptureVideoDataOutput* output;
}
- (void)start;
- (void)stop;
- (void)fail:(const char*)message;
@end

@implementation PrismWebcamCapture
- (void)fail:(const char*)message {
    std::lock_guard<std::mutex> lock(mutex);
    error=message;
}

- (void)start {
    if(cancelled.load())return;
    AVCaptureDevice* device=[AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if(!device){[self fail:"No webcam is available."];return;}
    AVCaptureDeviceInput* input=[AVCaptureDeviceInput deviceInputWithDevice:device error:nullptr];
    if(!input){[self fail:"Could not open the webcam."];return;}

    session=[[AVCaptureSession alloc] init];
    output=[[AVCaptureVideoDataOutput alloc] init];
    output.videoSettings=@{(__bridge NSString*)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA)};
    output.alwaysDiscardsLateVideoFrames=YES;
    [session beginConfiguration];
    if([session canSetSessionPreset:AVCaptureSessionPreset640x480])
        session.sessionPreset=AVCaptureSessionPreset640x480;
    if(![session canAddInput:input]){
        [session commitConfiguration];
        [self fail:"The webcam input is unavailable."];
        return;
    }
    [session addInput:input];
    if(![session canAddOutput:output]){
        [session commitConfiguration];
        [self fail:"The webcam video format is unavailable."];
        return;
    }
    [session addOutput:output];
    [output setSampleBufferDelegate:self queue:cameraQueue()];
    [session commitConfiguration];
    if(cancelled.load())return;
    [session startRunning];
    if(!session.running){[self fail:"The webcam could not start."];return;}
    std::lock_guard<std::mutex> lock(mutex);
    lastFrame=Clock::now();
}

- (void)stop {
    [output setSampleBufferDelegate:nil queue:nullptr];
    [session stopRunning];
    output=nil;
    session=nil;
}

- (void)captureOutput:(AVCaptureOutput*)source
 didOutputSampleBuffer:(CMSampleBufferRef)sample
        fromConnection:(AVCaptureConnection*)connection {
    if(cancelled.load())return;
    CVPixelBufferRef buffer=CMSampleBufferGetImageBuffer(sample);
    if(!buffer || CVPixelBufferGetPixelFormatType(buffer)!=kCVPixelFormatType_32BGRA ||
       CVPixelBufferLockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly)!=kCVReturnSuccess)return;
    const auto* base=static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(buffer));
    const size_t height=CVPixelBufferGetHeight(buffer),width=CVPixelBufferGetWidth(buffer);
    const size_t stride=CVPixelBufferGetBytesPerRow(buffer),row=width*4;
    if(base && width && height && stride>=row){
        std::lock_guard<std::mutex> lock(mutex);
        latest.width=int(width);
        latest.height=int(height);
        latest.bgra.resize(row*height);
        for(size_t y=0;y<height;++y)
            std::memcpy(latest.bgra.data()+y*row,base+y*stride,row);
        frameReady=true;
        lastFrame=Clock::now();
    }
    CVPixelBufferUnlockBaseAddress(buffer,kCVPixelBufferLock_ReadOnly);
}
@end

namespace prism {
struct Webcam::State {
    PrismWebcamCapture* capture=nil;
};

Webcam::Webcam():state(std::make_unique<State>()){}
Webcam::~Webcam(){
    setEnabled(false);
    dispatch_sync(cameraQueue(), ^{});
}

void Webcam::setEnabled(bool enabled) {
    PrismWebcamCapture* capture=state->capture;
    if(!enabled){
        if(!capture)return;
        capture->cancelled.store(true);
        state->capture=nil;
        dispatch_async(cameraQueue(), ^{[capture stop];});
        return;
    }
    if(capture)return;

    capture=[[PrismWebcamCapture alloc] init];
    capture->cancelled.store(false);
    state->capture=capture;
    const auto status=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if(status==AVAuthorizationStatusAuthorized){
        dispatch_async(cameraQueue(), ^{[capture start];});
    }else if(status==AVAuthorizationStatusNotDetermined){
        __weak PrismWebcamCapture* pending=capture;
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL allowed){
            PrismWebcamCapture* authorized=pending;
            if(!authorized || authorized->cancelled.load())return;
            if(allowed)dispatch_async(cameraQueue(), ^{[authorized start];});
            else [authorized fail:"Allow Camera access in System Settings to use the webcam."];
        }];
    }else{
        [capture fail:"Allow Camera access in System Settings to use the webcam."];
    }
}

bool Webcam::poll(WebcamFrame& frame,std::string& error) {
    PrismWebcamCapture* capture=state->capture;
    if(!capture)return false;
    std::lock_guard<std::mutex> lock(capture->mutex);
    error=capture->error;
    if(!error.empty())return false;
    const auto last=capture->lastFrame;
    if(last!=Clock::time_point{} && Clock::now()-last>std::chrono::seconds(6)){
        error="The webcam stopped sending frames.";
        return false;
    }
    if(!capture->frameReady)return false;
    capture->frameReady=false;
    std::swap(frame,capture->latest);
    return true;
}
}
