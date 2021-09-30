//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImguiUtils.h>
#include <Dalton/Image.h>
#include <Dalton/OpenGL.h>

struct GLFWwindow;

namespace dl
{

class GLTexture;

// CrossPlatform APIs

dl::Point getMouseCursor();

dl::Rect getFrontWindowGeometry(GLFWwindow* grabWindowHandle);

void setAppFocusEnabled (bool enabled);

class ScreenGrabber
{
public:
    ScreenGrabber ();
    ~ScreenGrabber ();
    
public:
    bool grabScreenArea (const dl::Rect& screenRect, dl::ImageSRGBA& cpuArea, GLTexture& gpuTexture);
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

class DisplayLinkTimer
{
public:
    DisplayLinkTimer ();
    ~DisplayLinkTimer ();
    
    void setCallback (const std::function<void(void)>& callback);
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

class KeyboardMonitor
{
public:
    KeyboardMonitor ();
    ~KeyboardMonitor ();
    
    // Bool to know if pressed or released.
    void setKeyboardCtrlAltCmdFlagsCallback (const std::function<void(bool)>& callback);
    
    void setKeyboardCtrlAltCmdSpaceCallback (const std::function<void(void)>& callback);

    void runOnce ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window);

void openURLInBrowser(const char* url);

void getVersionAndBuildNumber(std::string& version, std::string& build);

struct StartupManager
{
    StartupManager ();
    
    bool isLaunchAtStartupEnabled () const { return _isEnabled; }

    void setLaunchAtStartupEnabled (bool enabled);

private:
    bool _isEnabled;
};

} // dl
