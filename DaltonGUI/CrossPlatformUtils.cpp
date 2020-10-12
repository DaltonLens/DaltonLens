//
//  CrossPlatformUtils.cpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#include "CrossPlatformUtils.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <GLKit/GLKit.h>

#import "MASShortcut.h"
#import "MASShortcutMonitor.h"

namespace dl
{

struct DisplayLinkTimer::Impl
{
    NSTimer* timer = nil;
    std::function<void(void)> callback = nullptr;
};

DisplayLinkTimer::DisplayLinkTimer ()
: impl (new Impl())
{
}

DisplayLinkTimer::~DisplayLinkTimer ()
{
    
}

void DisplayLinkTimer::setCallback (const std::function<void(void)>& callback)
{
    if (impl->timer)
        [impl->timer invalidate];
    
    impl->callback = callback;
    
    impl->timer = [NSTimer scheduledTimerWithTimeInterval:1/60. repeats:YES block:^(NSTimer * _Nonnull timer) {
       if (impl->callback)
           impl->callback();
    }];
}

} // dl

namespace dl
{

struct KeyboardMonitor::Impl
{
    std::function<void(bool)> ctrlAltCmdFlagsCallback = nullptr;
    std::function<void(void)> ctrlAltCmdSpaceCallback = nullptr;
};

KeyboardMonitor::KeyboardMonitor ()
: impl (new Impl())
{
}

KeyboardMonitor::~KeyboardMonitor () = default;

// Bool to know if pressed or released.
void KeyboardMonitor::setKeyboardCtrlAltCmdFlagsCallback (const std::function<void(bool)>& callback)
{
    auto eventCallback = [=](NSEvent * _Nonnull event) {
        NSUInteger cmdControlAlt = NSEventModifierFlagCommand | NSEventModifierFlagControl | NSEventModifierFlagOption;
        
        // Make sure shift is not pressed.
        if ((event.modifierFlags & (cmdControlAlt | NSEventModifierFlagShift)) == cmdControlAlt)
        {
            callback(true);
        }
        else
        {
            callback(false);
        }
    };
    
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:^(NSEvent * _Nonnull event){
        eventCallback(event);
        return event;
    }];
    
    [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged handler:eventCallback];
}

void KeyboardMonitor::setKeyboardCtrlAltCmdSpaceCallback (const std::function<void(void)>& callback)
{
    NSUInteger cmdControlAlt = NSEventModifierFlagCommand | NSEventModifierFlagControl | NSEventModifierFlagOption;
    MASShortcut* shortcut = [MASShortcut shortcutWithKeyCode:kVK_Space modifierFlags:cmdControlAlt];
    
    [MASShortcutMonitor.sharedMonitor registerShortcut:shortcut withAction:[=]() {
        callback ();
    }];
}

} // dl

namespace dl
{

bool grabScreenArea (const dl::Rect& gpuRect, const dl::Rect& cpuRect, dl::ImageSRGBA& cpuArea, GLTexture& gpuTexture)
{
    CGRect screenRect = CGRectMake(gpuRect.origin.x, gpuRect.origin.y, gpuRect.size.x, gpuRect.size.y);
    
    CGImage* cgImage = CGWindowListCreateImage(screenRect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
    if (cgImage == nullptr)
    {
        return false;
    }
    
    NSError* error = nil;
    GLKTextureInfo* textureInfo = [GLKTextureLoader textureWithCGImage:cgImage options:@{} error:&error];
    gpuTexture.initializeWithExistingTextureID(textureInfo.name);
    return true;
}

} // dl

namespace dl
{

dl::Point getMouseCursor()
{
    NSPoint mousePos = [NSEvent mouseLocation];
    return dl::Point(mousePos.x, mousePos.y);
}

} // dl
