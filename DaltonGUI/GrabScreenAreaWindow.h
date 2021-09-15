//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/Graphics.h>
#include <DaltonGUI/ImguiGLFWWindow.h>

#include <Dalton/Image.h>
#include <Dalton/MathUtils.h>

#include "imgui.h"

#include <memory>
#include <functional>

namespace dl
{

struct GrabScreenData
{
    bool isValid = false;
    // Note: the capturedScreenRect does not have the same size as
    // the image on retina displays.
    dl::Rect capturedScreenRect;    
    std::shared_ptr<dl::ImageSRGBA> srgbaImage;
    std::shared_ptr<GLTexture> texture;
};

class ImageCursorOverlay
{
public:
    void showTooltip(const dl::ImageSRGBA &image,
                     GLTexture &imageTexture,
                     ImVec2 imageWidgetTopLeft,
                     ImVec2 imageWidgetSize,
                     const ImVec2 &uvTopLeft = ImVec2(0, 0),
                     const ImVec2 &uvBottomRight = ImVec2(1, 1),
                     const ImVec2 &roiWindowSize = ImVec2(15, 15));

private:
    double _timeOfLastCopyToClipboard = NAN;
};

// Manages a single ImGui window.
class GrabScreenAreaWindow
{
public:
    GrabScreenAreaWindow();
    ~GrabScreenAreaWindow();
    
public:
    bool initialize (GLFWwindow* parentContext);
    void shutdown ();
    void runOnce ();
    bool startGrabbing ();
    void dismiss ();
    
    bool isGrabbing() const;
    bool grabbingFinished() const;
    const GrabScreenData& grabbedData () const;
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
