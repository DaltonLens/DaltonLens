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

#if 0
bool ImageViewer::initialize (int argc, char** argv, GLFWwindow* parentWindow)
{
    dl::ScopeTimer initTimer ("Init");

    for (int i = 0; i < argc; ++i)
    {
        dl_dbg("%d: %s", i, argv[i]);
    }
    
    argparse::ArgumentParser parser("dlv", "0.1");
    parser.add_argument("images")
          .help("Images to visualize")
          .remaining();

    parser.add_argument("--paste")
          .help("Paste the image from clipboard")
          .default_value(false)
          .implicit_value(true);
    
    parser.add_argument("--geometry")
        .help("Geometry of the image area");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cerr << "Wrong usage" << std::endl;
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        return false;
    }

    try
    {
        auto images = parser.get<std::vector<std::string>>("images");
        dl_dbg("%d images provided", (int)images.size());

        impl->imagePath = images[0];
        bool couldLoad = dl::readPngImage(impl->imagePath, impl->im);
        dl_assert (couldLoad, "Could not load the image!");
    }
    catch (std::logic_error& e)
    {
        std::cerr << "No files provided" << std::endl;
    }

    if (parser.present<std::string>("--geometry"))
    {
        std::string geometry = parser.get<std::string>("--geometry");
        int x,y,w,h;
        const int count = sscanf(geometry.c_str(), "%dx%d+%d+%d", &w, &h, &x, &y);
        if (count == 4)
        {
            impl->imageWidgetRect.normal.size.x = w;
            impl->imageWidgetRect.normal.size.y = h;
            impl->imageWidgetRect.normal.origin.x = x;
            impl->imageWidgetRect.normal.origin.y = y;
        }
        else
        {
            std::cerr << "Invalid geometry string " << geometry << std::endl;
            std::cerr << "Format is WidthxHeight+X+Y" << geometry << std::endl;
            return false;
        }
    }
    
    if (parser.get<bool>("--paste"))
    {
        impl->imagePath = "Pasted from clipboard";

        if (!clip::has(clip::image_format()))
        {
            std::cerr << "Clipboard doesn't contain an image" << std::endl;
            return false;
        }

        clip::image clipImg;
        if (!clip::get_image(clipImg))
        {
            std::cout << "Error getting image from clipboard\n";
            return false;
        }

        clip::image_spec spec = clipImg.spec();

        std::cerr << "Image in clipboard "
            << spec.width << "x" << spec.height
            << " (" << spec.bits_per_pixel << "bpp)\n"
            << "Format:" << "\n"
            << std::hex
            << "  Red   mask: " << spec.red_mask << "\n"
            << "  Green mask: " << spec.green_mask << "\n"
            << "  Blue  mask: " << spec.blue_mask << "\n"
            << "  Alpha mask: " << spec.alpha_mask << "\n"
            << std::dec
            << "  Red   shift: " << spec.red_shift << "\n"
            << "  Green shift: " << spec.green_shift << "\n"
            << "  Blue  shift: " << spec.blue_shift << "\n"
            << "  Alpha shift: " << spec.alpha_shift << "\n";

        switch (spec.bits_per_pixel)
        {
        case 32:
        {
            impl->im.ensureAllocatedBufferForSize((int)spec.width, (int)spec.height);
            impl->im.copyDataFrom((uint8_t*)clipImg.data(), (int)spec.bytes_per_row, (int)spec.width, (int)spec.height);
            break;
        }

        case 16:
        case 24:
        case 64:
        default:
        {
            std::cerr << "Only 32bpp clipboard supported right now." << std::endl;
            return false;
        }
        }
    }
    
    initialize (parentWindow);

    if (impl->imageWidgetRect.normal.size.x < 0) impl->imageWidgetRect.normal.size.x = impl->im.width();
    if (impl->imageWidgetRect.normal.size.y < 0) impl->imageWidgetRect.normal.size.y = impl->im.height();
    if (impl->imageWidgetRect.normal.origin.x < 0) impl->imageWidgetRect.normal.origin.x = impl->monitorSize.x * 0.10;
    if (impl->imageWidgetRect.normal.origin.y < 0) impl->imageWidgetRect.normal.origin.y = impl->monitorSize.y * 0.10;
    impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
    
    impl->gpuTexture.upload (impl->im);
    impl->highlightRegion.setImage(&impl->im);
    
    return true;
}
#endif

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
