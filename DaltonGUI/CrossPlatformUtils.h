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

void grabScreenArea (const dl::Rect& gpuRect, const dl::Rect& cpuRect, dl::ImageSRGBA& cpuArea, GLTexture& gpuArea);

class DisplayLinkTimer
{
public:
    DisplayLinkTimer ();
    ~DisplayLinkTimer ();
    
    void setCallback (std::function<void(void)>);
    void setEnabled (bool enabled);
    
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
    
    void setKeyboardCtrlAltCmdFlagsCallback (bool keysDown);
    void setKeyboardCtrlAltCmdSpaceCallback ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

dl::Point getMouseCursor();

} // dl
