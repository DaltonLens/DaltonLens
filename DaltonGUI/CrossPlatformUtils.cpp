//
//  CrossPlatformUtils.cpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>

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

struct ScreenGrabber::Impl
{
    CGContextRef cgContext = nullptr;
    int cgContextWidth = -1;
    int cgContextHeight = -1;
    
    ~Impl()
    {
        maybeReleaseContext ();
    }
    
    void maybeReleaseContext ()
    {
        if (this->cgContext)
        {
            CGContextRelease(this->cgContext);
            this->cgContext = nullptr;
        }
    }
    
    void ensureCGContextCreatedForSize (int width, int height)
    {
        if (this->cgContextWidth != width || this->cgContextHeight != height)
        {
            createARGBBitmapContext(width, height);
        }
    }
    
    void createARGBBitmapContext (int width, int height)
    {
        maybeReleaseContext ();
        
        // Get image width, height. We'll use the entire image.
        int pixelsWide = width;
        int pixelsHigh = height;
        
        // Declare the number of bytes per row. Each pixel in the bitmap in this
        // example is represented by 4 bytes; 8 bits each of red, green, blue, and
        // alpha.
        int bitmapBytesPerRow = (pixelsWide * 4);
        int bitmapByteCount = (bitmapBytesPerRow * pixelsHigh);

        // Use the generic RGB color space.
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        dl_assert (colorSpace != nullptr, "Could not create the color space");
        
        // Allocate memory for image data. This is the destination in memory
        // where any drawing to the bitmap context will be rendered.
        uint8_t* bitmapData = (uint8_t*) malloc( bitmapByteCount );
        dl_assert (bitmapData != nullptr, "Memory allocation failed");
        
        // Create the bitmap context. We want pre-multiplied ARGB, 8-bits
        // per component. Regardless of what the source image format is
        // (CMYK, Grayscale, and so on) it will be converted over to the format
        // specified here by CGBitmapContextCreate.
        this->cgContext = CGBitmapContextCreate(bitmapData,
                                                pixelsWide,
                                                pixelsHigh,
                                                8, // bitsPerComponent
                                                bitmapBytesPerRow,
                                                colorSpace,
                                                kCGImageAlphaNoneSkipLast);
                
        dl_assert (this->cgContext != nullptr, "Context not created!");
        
        this->cgContextWidth = width;
        this->cgContextHeight = height;
        
        CGContextSetShouldAntialias(this->cgContext, false);
        CGContextSetInterpolationQuality(this->cgContext, kCGInterpolationNone);
    }
};
    
ScreenGrabber::ScreenGrabber ()
: impl (new Impl())
{
    
}

ScreenGrabber::~ScreenGrabber ()
{
}
    
bool ScreenGrabber::grabScreenArea (const dl::Rect& screenRect, dl::ImageSRGBA& cpuImage, GLTexture& gpuTexture)
{
    CGRect cgRect = CGRectMake(screenRect.origin.x, screenRect.origin.y, screenRect.size.x, screenRect.size.y);
    
    CGImage* cgImage = CGWindowListCreateImage(cgRect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
    if (cgImage == nullptr)
    {
        return false;
    }
    
    NSError* error = nil;
    GLKTextureInfo* textureInfo = [GLKTextureLoader textureWithCGImage:cgImage options:@{} error:&error];
    gpuTexture.initializeWithExistingTextureID(textureInfo.name);
        
    // Note: the image size is NOT the same as screenRect.size on retina displays.
    // So we need to get it from the texture to avoid losing resolution.
    const int imageWidth = textureInfo.width;
    const int imageHeight = textureInfo.height;
    impl->ensureCGContextCreatedForSize (imageWidth, imageHeight);
    
    CGContextDrawImage(impl->cgContext, CGRectMake(0, 0, imageWidth, imageHeight), cgImage);
    
    uint8_t* imageBuffer = (uint8_t*)CGBitmapContextGetData(impl->cgContext);
    
    cpuImage.ensureAllocatedBufferForSize (imageWidth, imageHeight);
    cpuImage.copyDataFrom(imageBuffer,
                         (int)CGBitmapContextGetBytesPerRow(impl->cgContext),
                         (int)CGBitmapContextGetWidth(impl->cgContext),
                         (int)CGBitmapContextGetHeight(impl->cgContext));
    
    CGImageRelease (cgImage);
    return true;
}

dl::Rect getFrontWindowGeometry()
{
    dl::Rect output;
    output.origin = dl::Point(-1,-1);
    output.size = dl::Point(-1,-1);
    
    CFArrayRef windowList_cf = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    NSArray* windowList = CFBridgingRelease(windowList_cf);
    const unsigned windowCount = windowList.count;
    
    dl::Point mousePos = getMouseCursor();
    mousePos.y = NSScreen.mainScreen.frame.size.height - mousePos.y;
    
    NSLog(@"Window list = %@", windowList);
    NSLog(@"mousePos = %f %f", mousePos.x, mousePos.y);
    
    //Iterate through the CFArrayRef and fill the vector
    for (int i = 0; i < windowCount ; ++i)
    {
        NSDictionary* dict = (NSDictionary*)[windowList objectAtIndex:i];
        int layer = [(NSNumber*)[dict objectForKey:@"kCGWindowLayer"] intValue];
        
        NSString* owner = (NSString*)[dict objectForKey:@"kCGWindowOwnerName"];
        if ([owner isEqualToString:@"DaltonLens"])
            continue;
        
        if (layer == 0)
        {
            NSDictionary* bounds = (NSDictionary*)[dict objectForKey:@"kCGWindowBounds"];
            dl::Rect windowRect;
            windowRect.size.x = [(NSNumber*)bounds[@"Width"] intValue];
            windowRect.size.y = [(NSNumber*)bounds[@"Height"] intValue];
            windowRect.origin.x = [(NSNumber*)bounds[@"X"] intValue];
            windowRect.origin.y = [(NSNumber*)bounds[@"Y"] intValue];
            
            if (windowRect.contains(mousePos))
            {
                output = windowRect;
                break;
            }
            else if (output.size.x < 0)
            {
                // If the mouse is not over any window, save at least first window that had valid bounds
                // since the windows come in order, the front-most first.
                output = windowRect;
            }
        }
    }
    
    return output;
}

} // dl

namespace dl
{

void setAppFocusEnabled (bool enabled)
{
    NSApp.activationPolicy = enabled ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
}

dl::Point getMouseCursor()
{
    NSPoint mousePos = [NSEvent mouseLocation];
    return dl::Point(mousePos.x, mousePos.y);
}

} // dl
