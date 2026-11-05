#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "audio.hpp"

namespace prism {
std::string chooseAudioFile() {
    @autoreleasepool {
        NSWindow* host=[NSApp keyWindow];
        NSOpenPanel* panel=[NSOpenPanel openPanel];
        [panel setTitle:@"Open audio in Prism Currents"];
        NSMutableArray<UTType*>* types=[NSMutableArray array];
        for(NSString* extension in @[@"wav",@"mp3",@"flac"]){
            UTType* type=[UTType typeWithFilenameExtension:extension];if(type)[types addObject:type];
        }
        [panel setAllowedContentTypes:types];
        [panel setCanChooseDirectories:NO];[panel setAllowsMultipleSelection:NO];
        std::string path;
        if([panel runModal]==NSModalResponseOK)path=[[[panel URL] path] UTF8String];
        [host makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
        return path;
    }
}
std::string choosePngFile(const std::string& suggested) {
    @autoreleasepool {
        NSWindow* host=[NSApp keyWindow];NSSavePanel* panel=[NSSavePanel savePanel];
        [panel setTitle:@"Save Prism Currents image"];
        [panel setAllowedContentTypes:@[[UTType typeWithFilenameExtension:@"png"]]];
        NSString* path=[NSString stringWithUTF8String:suggested.c_str()];
        [panel setNameFieldStringValue:[path lastPathComponent]];
        [panel setDirectoryURL:[NSURL fileURLWithPath:[path stringByDeletingLastPathComponent]]];
        std::string result;if([panel runModal]==NSModalResponseOK)result=[[[panel URL] path] UTF8String];
        [host makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];return result;
    }
}
bool microphoneAccess() {
    auto status=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if(status==AVAuthorizationStatusNotDetermined) {
        __block bool finished=false;__block bool allowed=false;
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL value){
            dispatch_async(dispatch_get_main_queue(),^{allowed=value;finished=true;});
        }];
        while(!finished) [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.03]];
        return allowed;
    }
    return status==AVAuthorizationStatusAuthorized;
}
}
