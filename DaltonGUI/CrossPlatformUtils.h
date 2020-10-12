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

namespace dl
{

// CrossPlatform APIs

bool grabScreenArea (const dl::Rect& gpuRect, const dl::Rect& cpuRect, dl::ImageSRGBA& cpuArea, GLTexture& gpuTexture);

dl::Point getMouseCursor();

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

} // dl
