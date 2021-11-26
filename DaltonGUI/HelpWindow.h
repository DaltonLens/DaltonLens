//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImguiGLFWWindow.h>

#include <memory>
#include <functional>

struct GLFWwindow;

namespace dl
{

class HelpWindow
{
public:
    HelpWindow();
    ~HelpWindow();

    bool initialize (GLFWwindow* parentWindow);
    void runOnce ();

public:
    void shutdown ();
    void setEnabled (bool enabled);
    bool isEnabled () const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
