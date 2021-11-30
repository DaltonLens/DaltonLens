//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "PlatformSpecific.h"

#include <Dalton/Utils.h>

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <GLKit/GLKit.h>

#import "MASShortcut.h"
#import "MASShortcutMonitor.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA 1
#include <GLFW/glfw3native.h>

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

void KeyboardMonitor::runOnce ()
{
    // do nothing, callback based on macOS
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
    
// From https://stackoverflow.com/a/58786245/1737680
BOOL canRecordScreen ()
{
    static BOOL canRecordScreen = NO;
    
    // Permission was granted, let's assume that if was not disabled in
    // the lifetime of the app since a permission change would restart it
    // anyway.
    if (canRecordScreen)
        return canRecordScreen;
    
    if (@available(macOS 10.15, *))
    {
        canRecordScreen = NO;
        NSRunningApplication *runningApplication = NSRunningApplication.currentApplication;
        NSNumber *ourProcessIdentifier = [NSNumber numberWithInteger:runningApplication.processIdentifier];

        CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
        NSUInteger numberOfWindows = CFArrayGetCount(windowList);
        for (int index = 0; index < numberOfWindows; index++) {
            // get information for each window
            NSDictionary *windowInfo = (NSDictionary *)CFArrayGetValueAtIndex(windowList, index);
            NSString *windowName = windowInfo[(id)kCGWindowName];
            NSNumber *processIdentifier = windowInfo[(id)kCGWindowOwnerPID];

            // don't check windows owned by this process
            if (! [processIdentifier isEqual:ourProcessIdentifier]) {
                // get process information for each window
                pid_t pid = processIdentifier.intValue;
                NSRunningApplication *windowRunningApplication = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                if (! windowRunningApplication) {
                    // ignore processes we don't have access to, such as WindowServer, which manages the windows named "Menubar" and "Backstop Menubar"
                }
                else {
                    NSString *windowExecutableName = windowRunningApplication.executableURL.lastPathComponent;
                    if (windowName) {
                        if ([windowExecutableName isEqual:@"Dock"]) {
                            // ignore the Dock, which provides the desktop picture
                        }
                        else {
                            canRecordScreen = YES;
                            break;
                        }
                    }
                }
            }
        }
        CFRelease(windowList);
    }
    else
    {
        // Before Catalina it was always fine.
        canRecordScreen = YES;
    }
    
    return canRecordScreen;
}

bool ScreenGrabber::grabScreenArea (const dl::Rect& screenRect, dl::ImageSRGBA& cpuImage, GLTexture& gpuTexture)
{
    CGRect cgRect = CGRectMake(screenRect.origin.x, screenRect.origin.y, screenRect.size.x, screenRect.size.y);

    if (!canRecordScreen())
    {
        NSTextView *accessory = [[NSTextView alloc] initWithFrame:NSMakeRect(0,0,200,15)];
        NSString* msg = @"DaltonLens needs to capture your screen to apply color filters.\n\n"
                        "To give it permission you need to enable the DaltonLens app in "
                        "System Preferences / Security & Privacy / Screen Recording.\n\n"
                        "If this is the first time that you see this dialog, a macOS"
                        " prompt will propose to take you there directly once you click on OK.";
        NSFont *font = [NSFont systemFontOfSize:[NSFont systemFontSize]];
        NSDictionary *textAttributes = [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
        [accessory insertText:[[NSAttributedString alloc] initWithString:msg attributes:textAttributes]];
        [accessory setEditable:NO];
        [accessory setDrawsBackground:NO];
        accessory.alignment = NSTextAlignmentJustified;
        
        NSAlert* alert = [NSAlert new];
        [alert setMessageText: @"Screen recording permission required."];
        [alert setAlertStyle:NSAlertStyleCritical];
        alert.accessoryView = accessory;
        [alert runModal];
        
        // Make sure we'll trigger a prompt.
        CGImage* cgImage = CGWindowListCreateImage(cgRect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
        CGImageRelease (cgImage);
    
        return false;
    }
    
    CGImage* cgImage = CGWindowListCreateImage(cgRect, kCGWindowListOptionOnScreenOnly, kCGNullWindowID, kCGWindowImageDefault);
    if (cgImage == nullptr)
    {
        return false;
    }
    
    NSError* error = nil;
    GLKTextureInfo* textureInfo = [GLKTextureLoader textureWithCGImage:cgImage options:@{} error:&error];
    gpuTexture.initializeWithExistingTextureID(textureInfo.name, textureInfo.width, textureInfo.height);
        
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

dl::Rect getFrontWindowGeometry(GLFWwindow* grabWindowHandle)
{
    dl::Rect output;
    output.origin = dl::Point(-1,-1);
    output.size = dl::Point(-1,-1);
    
    CFArrayRef windowList_cf = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    NSArray* windowList = CFBridgingRelease(windowList_cf);
    const unsigned windowCount = (unsigned)windowList.count;
    
    dl::Point mousePos = getMouseCursor();
    mousePos.y = NSScreen.mainScreen.frame.size.height - mousePos.y;
    
    const auto monitorBounds = dl::Rect::from_x_y_w_h(0,
                                                      0,
                                                      NSScreen.mainScreen.frame.size.width,
                                                      NSScreen.mainScreen.frame.size.height);
    
    //    NSLog(@"mousePos: %f %f", mousePos.x, mousePos.y);
    //    NSLog(@"Window list = %@", windowList);
    //    NSLog(@"mousePos = %f %f", mousePos.x, mousePos.y);
    
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
                // The intersection makes sure that we won't get negative coordinates
                // if the window is partially hidden. This would make the area look
                // invalid.
                output = windowRect.intersect (monitorBounds);
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
    
    //    NSLog(@"output origin: %f %f", output.origin.x, output.origin.y);
    //    NSLog(@"output size: %f %f", output.size.x, output.size.y);
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

void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window)
{
    NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(window);
    dl_assert (nsWindow, "Not working?");
    nsWindow.collectionBehavior = (nsWindow.collectionBehavior
                                   // | NSWindowCollectionBehaviorCanJoinAllSpaces
                                   | NSWindowCollectionBehaviorMoveToActiveSpace);
}

void openURLInBrowser(const char* url)
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

void getVersionAndBuildNumber(std::string& version, std::string& build)
{
    // https://stackoverflow.com/questions/10015304/refer-to-build-number-or-version-number-in-code
    NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
    NSString *appVersion = [infoDict objectForKey:@"CFBundleShortVersionString"]; // example: 1.0.0
    NSString *buildNumber = [infoDict objectForKey:@"CFBundleVersion"]; // example: 42
    version = [appVersion UTF8String];
    build = [buildNumber UTF8String];
}

} // dl
