//
//  DaltonLensGUI.hpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#pragma once

#include <memory>
#include <functional>

namespace dl
{

class DaltonLensGUI
{
public:
    DaltonLensGUI();
    ~DaltonLensGUI();
    
    // Call it once per app, calls glfwInit, etc.
    // Sets up the callback on keyboard flags
    // Sets up the hotkeys
    bool initialize ();
    
    void runOnce ();
    
    void helpRequested ();
    
    void grabScreenArea ();
    
    void shutdown ();

private:
    enum CurrentMode
    {
        GlobalCursorOverlay,
        ViewerWindow,
        GrabScreen,
    };
    
private:
    void onKeyboardFlagsChanged ();
    void onHotkeyPressed ();
    void onDisplayLinkUpdated ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
