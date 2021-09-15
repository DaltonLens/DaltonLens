//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImageViewerObserver.h>

#include <Dalton/MathUtils.h>

#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

struct GrabScreenData;

class ImageViewerWindowState;

// Manages a single ImGuiWindow
class ImageViewerWindow
{
public:
    ImageViewerWindow();
    ~ImageViewerWindow();
    
public:   
    bool initialize (GLFWwindow* parentWindow, ImageViewerObserver* observer);
    
    void showGrabbedData (const GrabScreenData& grabbedData, dl::Rect& updatedWindowGeometry);
    
    void shutdown ();
    void runOnce ();
    
    bool isEnabled () const;
    void setEnabled (bool enabled);

    dl::Rect geometry () const;

    void processKeyEvent (int keycode);
    void checkImguiGlobalImageKeyEvents ();

public:
    // State that one can modify directly between frames.
    ImageViewerWindowState& mutableState ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
