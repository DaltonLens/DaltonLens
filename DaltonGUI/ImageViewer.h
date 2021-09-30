//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/ImageViewerObserver.h>

#include <memory>
#include <functional>

struct GLFWwindow;

namespace dl
{

struct GrabScreenData;

// Manages a single ImGuiWindow
class ImageViewer : public ImageViewerObserver
{
public:
    ImageViewer();
    ~ImageViewer();
    
public:
#if 0
    // Aborted attempt, keeping it around in case it proves useful one day.
    bool initialize (int argc, char** argv, GLFWwindow* parentWindow);
#endif
    
    bool initialize (GLFWwindow* parentWindow);
    
    void showGrabbedData (const GrabScreenData& grabbedData);
    
    void shutdown ();
    void runOnce ();
    
    bool isEnabled () const;
    void setEnabled (bool enabled);
    
    bool helpWindowRequested () const;
    void notifyHelpWindowRequestHandled ();

public:
    // Observer methods.
    virtual void onDismissRequested () override;
    virtual void onHelpRequested () override;
    virtual void onControlsRequested () override;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
