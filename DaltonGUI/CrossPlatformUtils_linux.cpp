//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>
#include <Dalton/OpenGL.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11 1
#include <GLFW/glfw3native.h>

#include <xdo/xdo_mini.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <thread>
#include <mutex>

// getpid
#include <sys/types.h>
#include <unistd.h>

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
    Display* display = glfwGetX11Display();
    if (!display)
        return false;

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
     
    return img != nullptr;
}



struct WindowUnderPointerFinder
{
    WindowUnderPointerFinder ()
    {
        display = glfwGetX11Display();
        root = DefaultRootWindow(display);
        atom_wmstate = XInternAtom(display, "WM_STATE", False);
        daltonLens_pid = getpid();
    }

    ~WindowUnderPointerFinder ()
    {
    }

    struct WindowInfo
    {
        Window w = 0;
        dl::Rect rect;
        std::string name;
    };

    dl::Rect findWindowGeometryUnderPointer ()
    {
        Window root_return = 0;
        Window child_return = 0;
        int root_x_return = 0, root_y_return = 0, win_x_return = 0, win_y_return = 0;
        unsigned int mask_return = 0;
        Bool success = XQueryPointer(display, root,
                                     &root_return, &child_return,
                                     &root_x_return, &root_y_return,
                                     &win_x_return, &win_y_return,
                                     &mask_return);
        if (success == 0)
        {
            return dl::Rect();
        }

        std::vector<WindowInfo> acc; acc.reserve (256);
        dl::Point pointer (win_x_return, win_y_return);
        appendCompatibleChildren (acc, root, pointer);
        if (acc.empty())
        {
            return dl::Rect();
        }

        WindowInfo frontMost = acc.back();

        if (frontMost.rect.area() > 0)
        {
            return frontMost.rect;
        }

        return dl::Rect();
    }

    void appendCompatibleChildren(std::vector<WindowInfo> &acc, Window parent, const dl::Point& pointer)
    {
        Window root_return = 0;
        Window parent_return = 0;
        unsigned int numChildren = 0;
        Window *children = nullptr;
        XQueryTree(display, parent, &root_return, &parent_return, &children, &numChildren);

        for (int i = 0; i < numChildren; ++i)
        {
            Window child = children[i];

            XWindowAttributes attributes;
            XGetWindowAttributes(display, parent, &attributes);
            if (attributes.map_state != IsViewable)
                continue;

            dl::Rect rect = dlRectFromAttributes (attributes, parent);
            if (!rect.contains(pointer))
                continue;

            long items = 0;
            xdo_get_window_property_by_atom(display, child, atom_wmstate, &items, NULL, NULL);

            if (items > 0)
            {
                const int window_pid = xdo_get_pid_window (display, child);
                if (window_pid == daltonLens_pid)
                {
                    dl_dbg("Discarding window with PID (%d)", window_pid);
                    continue;
                }

                unsigned char* name_ret_unsigned = nullptr;
                int name_len_ret = 0;
                int name_type = 0;
                xdo_get_window_name(display,
                                    child,
                                    &name_ret_unsigned,
                                    &name_len_ret,
                                    &name_type);
                char* name_ret = reinterpret_cast<char*>(name_ret_unsigned);             

                if (name_ret)
                {
                    WindowInfo info;
                    info.w = child;
                    info.rect = rect;
                    info.name = std::string(name_ret);
                    acc.push_back (info);
                }

                XFree(name_ret);
            }

            appendCompatibleChildren (acc, child, pointer);
        }

        XFree(children);
    }

    dl::Rect dlRectFromAttributes(const XWindowAttributes &attributes, Window parent) const
    {
        int x = attributes.x, y = attributes.y;
        if (parent != root)
        {
            Window unused_child;
            XTranslateCoordinates(display, parent, root,
                                  0, 0, &x, &y, &unused_child);
        }

        dl::Rect rect;
        rect.origin.x = x;
        rect.origin.y = y;
        rect.size.x = attributes.width;
        rect.size.y = attributes.height;
        return rect;
    }

    Display *display = nullptr;
    Window root = 0;
    Atom atom_wmstate;
    pid_t daltonLens_pid = 0;
};

dl::Rect getFrontWindowGeometry()
{
    WindowUnderPointerFinder finder;
    return finder.findWindowGeometryUnderPointer ();
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
    dl_assert (false, "Not implemented");
    return dl::Point(0,0);
}



void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window)
{
    Display* display = glfwGetX11Display();
    Window w = glfwGetX11Window (window);
    xdo_set_desktop_for_window (display, w, -1 /* all desktops */);

    
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
