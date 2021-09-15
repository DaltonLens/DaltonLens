//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11 1
#include <GLFW/glfw3native.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <thread>
#include <mutex>

namespace dl
{

struct DisplayLinkTimer::Impl
{};


DisplayLinkTimer::DisplayLinkTimer ()
: impl (new Impl())
{
}

DisplayLinkTimer::~DisplayLinkTimer ()
{
    
}

void DisplayLinkTimer::setCallback (const std::function<void(void)>& callback)
{
    dl_assert (false, "not implemented");
}

} // dl

namespace dl
{

struct KeyboardMonitor::Impl
{
    struct Shortcut
    {
        unsigned int modifiers = 0;
        int keycode = -1;
        std::function<void(void)> callback = nullptr;
    };

    Display* dpy = XOpenDisplay(0);
    Window root = DefaultRootWindow(dpy);

    Shortcut ctrlCmdAltSpace;
};

KeyboardMonitor::KeyboardMonitor ()
: impl (new Impl())
{
    impl->ctrlCmdAltSpace.modifiers = ControlMask | Mod1Mask | Mod4Mask;
    impl->ctrlCmdAltSpace.keycode = XKeysymToKeycode(impl->dpy, XK_space);

    impl->dpy = XOpenDisplay(0);
    impl->root = DefaultRootWindow(impl->dpy);

    Bool owner_events = False;
    int pointer_mode = GrabModeAsync;
    int keyboard_mode = GrabModeAsync;

    for (const auto& shortcut : { impl->ctrlCmdAltSpace } )
    {
        for (const auto modifiers : { shortcut.modifiers, shortcut.modifiers | LockMask, shortcut.modifiers | Mod2Mask, shortcut.modifiers | LockMask | Mod2Mask })
        {
            XGrabKey(impl->dpy, shortcut.keycode, modifiers, impl->root, owner_events, pointer_mode, keyboard_mode);
        }
    }

    XSelectInput(impl->dpy, impl->root, KeyPressMask);
}

KeyboardMonitor::~KeyboardMonitor ()
{
    for (const auto &shortcut : { impl->ctrlCmdAltSpace })
    {
        for (const auto modifiers : { shortcut.modifiers, shortcut.modifiers | LockMask, shortcut.modifiers | Mod2Mask, shortcut.modifiers | LockMask | Mod2Mask })
        {
            XUngrabKey(impl->dpy, shortcut.keycode, modifiers, impl->root);
        }
    }

    XCloseDisplay(impl->dpy);
}

// Bool to know if pressed or released.
void KeyboardMonitor::setKeyboardCtrlAltCmdFlagsCallback (const std::function<void(bool)>& callback)
{
    dl_assert (false, "not implemented on Linux");
}

void KeyboardMonitor::setKeyboardCtrlAltCmdSpaceCallback (const std::function<void(void)>& callback)
{
    impl->ctrlCmdAltSpace.callback = callback;
}

void KeyboardMonitor::runOnce ()
{
    if (!XPending(impl->dpy))
        return;

    XEvent ev;
    XNextEvent(impl->dpy, &ev);
    switch (ev.type)
    {
    case KeyPress:
    {
        dl_dbg ("Hot key pressed!");
        if (ev.xkey.keycode == impl->ctrlCmdAltSpace.keycode)
        {
            if (impl->ctrlCmdAltSpace.callback)
                impl->ctrlCmdAltSpace.callback();
        }
        break;
    }

    default:
        break;
    }
}

} // dl

namespace dl
{

struct ScreenGrabber::Impl
{
    ~Impl()
    {
        
    }
};
    
ScreenGrabber::ScreenGrabber ()
: impl (new Impl())
{
    
}

ScreenGrabber::~ScreenGrabber ()
{
}
    
// https://stackoverflow.com/questions/24988164/c-fast-screenshots-in-linux-for-use-with-opencv/39781697
bool ScreenGrabber::grabScreenArea (const dl::Rect& screenRect, dl::ImageSRGBA& cpuImage, GLTexture& gpuTexture)
{
    // FIXME: is it slow to open/close the display everytime? Could keep it open?
    Display* display = XOpenDisplay(nullptr);
    Window root = DefaultRootWindow(display);

    XWindowAttributes attributes = {0};
    XGetWindowAttributes(display, root, &attributes);

    XImage* img = XGetImage(display, root, screenRect.origin.x, screenRect.origin.y, screenRect.size.x, screenRect.size.y, AllPlanes, ZPixmap);
    dl_assert (img, "Could not capture the screen");
    
    if (img)
    {
        dl_assert (img->bits_per_pixel == 32, "Only RGBA is supported.");

        // It's easy to do & xxx_mask but then we need the corresponding bit shift otherwise
        // it gives a false sense of genericity. Could be done via a trivial lookup table.
        // But for now let's just assume that these values are accurate often enough to keep
        // the max performance.
        dl_assert (img->blue_mask == 0xff, "The code is not generic enough to handle something else properly.");
        dl_assert (img->green_mask == 0xff00, "The code is not generic enough to handle something else properly.");
        dl_assert (img->red_mask == 0xff0000, "The code is not generic enough to handle something else properly.");

        cpuImage.ensureAllocatedBufferForSize(screenRect.size.x, screenRect.size.y);    

        for (int r = 0; r < screenRect.size.y; ++r)
        {
            const uint32_t* srcRowPtr = reinterpret_cast<const uint32_t*>(img->data + img->bytes_per_line*r);
            PixelSRGBA* dstRowPtr = cpuImage.atRowPtr(r);
            const PixelSRGBA* lastDstRowPtr = cpuImage.atRowPtr(r + 1);
            while (dstRowPtr < lastDstRowPtr)
            {
                dstRowPtr->b = (*srcRowPtr & 0xff);
                dstRowPtr->g = (*srcRowPtr & 0xff00) >> 8;
                dstRowPtr->r = (*srcRowPtr & 0xff0000) >> 16;
                dstRowPtr->a = 255;
                ++srcRowPtr;
                ++dstRowPtr;
            }
        }

        // FIXME: is there a reliable way to grab the screenshot directly to a GL texture?
        gpuTexture.initialize ();
        gpuTexture.upload(cpuImage);

        XDestroyImage(img);
    }
     
    XCloseDisplay(display);
    return img != nullptr;
}

dl::Rect getFrontWindowGeometry()
{
#if 0
    dl::Rect output;
    output.origin = dl::Point(-1,-1);
    output.size = dl::Point(-1,-1);
    
    CFArrayRef windowList_cf = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    NSArray* windowList = CFBridgingRelease(windowList_cf);
    const unsigned windowCount = (unsigned)windowList.count;
    
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
#endif
    return dl::Rect();
}

} // dl

namespace dl
{

void setAppFocusEnabled (bool enabled)
{    
    // NSApp.activationPolicy = enabled ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory;
}

dl::Point getMouseCursor()
{
    // NSPoint mousePos = [NSEvent mouseLocation];
    // return dl::Point(mousePos.x, mousePos.y);
    return dl::Point(0,0);
}

void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window)
{
    // NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(window);
    // dl_assert (nsWindow, "Not working?");
    // nsWindow.collectionBehavior = nsWindow.collectionBehavior | NSWindowCollectionBehaviorMoveToActiveSpace;
}

void openURLInBrowser(const char* url)
{
    // [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

void getVersionAndBuildNumber(std::string& version, std::string& build)
{
    // https://stackoverflow.com/questions/10015304/refer-to-build-number-or-version-number-in-code
    // NSDictionary *infoDict = [[NSBundle mainBundle] infoDictionary];
    // NSString *appVersion = [infoDict objectForKey:@"CFBundleShortVersionString"]; // example: 1.0.0
    // NSString *buildNumber = [infoDict objectForKey:@"CFBundleVersion"]; // example: 42
    // version = [appVersion UTF8String];
    // build = [buildNumber UTF8String];
    version = "1.0";
    build = "linux";
}

} // dl
