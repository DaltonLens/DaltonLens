//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImageViewerController.h>

#include <Dalton/MathUtils.h>

#include <memory>
#include <functional>

struct GLFWwindow;

namespace dl
{

struct GrabScreenData;

struct ImageViewerWindowState;
struct CursorOverlayInfo;

// Manages a single ImGuiWindow
class ImageViewerWindow
{
public:
    ImageViewerWindow();
    ~ImageViewerWindow();
    
public:   
    bool initialize (GLFWwindow* parentWindow, ImageViewerController* controller);
    
    void showGrabbedData (const GrabScreenData& grabbedData, dl::Rect& updatedWindowGeometry);
    
    const CursorOverlayInfo& cursorOverlayInfo() const;
    
    void shutdown ();
    void runOnce ();
    
    bool isEnabled () const;
    void setEnabled (bool enabled);

    dl::Rect geometry () const;

    void processKeyEvent (int keycode);
    void checkImguiGlobalImageKeyEvents ();
    void checkImguiGlobalImageMouseEvents ();
    void saveCurrentImage ();

public:
    // State that one can modify directly between frames.
    ImageViewerWindowState& mutableState ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
