//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <memory>
#include <functional>

class GLFWwindow;

namespace dl
{

struct GrabScreenData;

// Manages a single ImGuiWindow
class ImageViewerWindow
{
public:
    ImageViewerWindow();
    ~ImageViewerWindow();
    
public:
#if 0
    // Aborted attempt, keeping it around in case it proves useful one day.
    bool initialize (int argc, char** argv, GLFWwindow* parentWindow);
#endif
    
    bool initialize (GLFWwindow* parentWindow);
    
    void showGrabbedData (const GrabScreenData& grabbedData);
    
    void shutdown ();
    void runOnce ();
    bool shouldExit () const;
    
    bool isEnabled () const;
    void dismiss ();
    
    bool helpWindowRequested () const;
    void notifyHelpWindowRequestHandled ();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
