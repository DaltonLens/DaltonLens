//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImguiGLFWWindow.h>
#include <DaltonGUI/ImageViewerController.h>

#include <memory>
#include <functional>

namespace dl
{

class ImageViewerWindow;

struct ControlsWindowInputState
{
    bool shiftIsPressed = false;
};

class ImageViewerControlsWindow
{    
public:
    ImageViewerControlsWindow();
    ~ImageViewerControlsWindow();

public:
    const ControlsWindowInputState& inputState () const;

public:
    bool initialize (GLFWwindow* parentWindow, ImageViewerController* controller);
    void runOnce ();
    void repositionAfterNextRendering (const dl::Rect& viewerWindowGeometry, bool showRequested);

public:
    void shutdown ();
    void setEnabled (bool enabled);
    bool isEnabled () const;    
    void bringToFront ();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};


} // dl
