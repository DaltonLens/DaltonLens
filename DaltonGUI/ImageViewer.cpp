//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#define GL_SILENCE_DEPRECATION 1

#include "ImageViewer.h"
#include "GrabScreenAreaWindow.h"

#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/PlatformSpecific.h>

#include "imgui.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/ColorConversion.h>

#include <DaltonGUI/ImageViewerWindow.h>
#include <DaltonGUI/ImageViewerControlsWindow.h>

#include <argparse.hpp>

#include <clip/clip.h>

#include <cstdio>

namespace dl
{

struct ImageViewer::Impl
{
    ImageViewerWindow imageWindow;
    ImageViewerControlsWindow controlsWindow;
    bool helpWindowRequested = false;
    bool enabled = false;
    bool dismissRequested = false;
};

ImageViewer::ImageViewer()
: impl (new Impl())
{}

ImageViewer::~ImageViewer()
{
    shutdown();
}

// Observer methods.
void ImageViewer::onDismissRequested ()
{
    // Delay the request until the end of frame since it might involve rendering
    // an extra frame e.g. in ImageWindow.
    impl->dismissRequested = true;
}

bool ImageViewer::isEnabled () const
{
    return impl->enabled;
}

void ImageViewer::setEnabled (bool enabled)
{
    impl->enabled = enabled;
    impl->controlsWindow.setEnabled (enabled);
    impl->imageWindow.setEnabled (enabled);
}

void ImageViewer::onHelpRequested ()
{
    impl->helpWindowRequested = true;
}

bool ImageViewer::helpWindowRequested () const
{
    return impl->helpWindowRequested;
}

void ImageViewer::notifyHelpWindowRequestHandled ()
{
    impl->helpWindowRequested = false;
}

void ImageViewer::onControlsRequested ()
{
    if (!impl->controlsWindow.isEnabled())
    {
        impl->controlsWindow.setEnabled(true);
    }
    else
    {
        impl->controlsWindow.bringToFront();
    }    
}

void ImageViewer::shutdown()
{
    impl->imageWindow.shutdown ();
    impl->controlsWindow.shutdown ();
}

bool ImageViewer::initialize (GLFWwindow* parentWindow)
{
    if (!impl->imageWindow.initialize (parentWindow, this /* controller */))
        return false;
    
    if (!impl->controlsWindow.initialize (parentWindow, this /* controller */))
        return false;

    return true;
}

void ImageViewer::showGrabbedData (const GrabScreenData& grabbedData)
{
    dl::Rect updatedViewerWindowGeometry;
    impl->imageWindow.showGrabbedData (grabbedData, updatedViewerWindowGeometry);
    impl->controlsWindow.repositionAfterNextRendering (updatedViewerWindowGeometry, true /* show by default */);
    
    // Not calling setEnabled on purpose, to avoid showing the window
    // before we do one render pass and thus potentially before it got moved.
    impl->enabled = true;
}

void ImageViewer::runOnce ()
{
    if (impl->enabled)
    {
        impl->imageWindow.runOnce();
        impl->controlsWindow.runOnce();
    }

    if (impl->dismissRequested)
    {
        setEnabled (false);
        impl->dismissRequested = false;
    }
}

ImageViewerWindow* ImageViewer::activeViewerWindow ()
{
    return &impl->imageWindow;
}

ImageViewerControlsWindow* ImageViewer::controlsWindow ()
{
    return &impl->controlsWindow;
}

} // dl
