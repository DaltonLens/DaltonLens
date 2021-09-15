//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImguiGLFWWindow.h>

#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

class HelpWindow
{
public:
    bool initialize (GLFWwindow* parentWindow);
    void runOnce ();

public:
    void shutdown () { _imguiGlfwWindow.shutdown (); }
    void setEnabled (bool enabled) { _imguiGlfwWindow.setEnabled (enabled); }
    bool isEnabled () const { return _imguiGlfwWindow.isEnabled(); }
    
private:
    // Debatable, but decided to use composition for more flexibility and explicit code.
    ImguiGLFWWindow _imguiGlfwWindow;
};

} // dl
