//
//  CrossPlatformUtils.hpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#pragma once

#include <DaltonGUI/ImGuiUtils.h>
#include <Dalton/Image.h>
#include "Graphics.h"

struct GLFWwindow;

namespace dl
{

// CrossPlatform APIs

dl::Point getMouseCursor();

dl::Rect getFrontWindowGeometry();

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
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

void setWindowFlagsToAlwaysShowOnActiveDesktop(GLFWwindow* window);

} // dl
