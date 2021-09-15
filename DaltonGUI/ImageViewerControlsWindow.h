//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImguiGLFWWindow.h>
#include <DaltonGUI/ImageViewerObserver.h>

#include <memory>
#include <functional>

namespace dl
{

class ImageViewerWindow;

class ImageViewerControlsWindow
{    
public:
    ImageViewerControlsWindow();
    ~ImageViewerControlsWindow();

    bool initialize (GLFWwindow* parentWindow, ImageViewerObserver* observer);
    void runOnce (ImageViewerWindow* activeImageWindow);

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
