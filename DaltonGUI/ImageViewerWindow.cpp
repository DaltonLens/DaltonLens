//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#define GL_SILENCE_DEPRECATION 1

#include "ImageViewerWindow.h"
#include "ImageViewerWindowState.h"

#include <DaltonGUI/GrabScreenAreaWindow.h>
#include <DaltonGUI/ImageCursorOverlay.h>
#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/PlatformSpecific.h>
#include <DaltonGUI/ImguiGLFWWindow.h>
#include <DaltonGUI/HighlightSimilarColor.h>
#include <DaltonGUI/ImageViewerControlsWindow.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/ColorConversion.h>
#include <Dalton/Filters.h>

// Note: need to include that before GLFW3 for some reason.
#include <GL/gl3w.h>
#include <DaltonGUI/GLFWUtils.h>

#include <nfd.h>

#include <argparse.hpp>

#include <clip/clip.h>

#include <cstdio>

namespace dl
{

bool viewerModeIsDaltonize (DaltonViewerMode mode)
{
    switch (mode)
    {
        case DaltonViewerMode::Protanope:
        case DaltonViewerMode::Deuteranope:
        case DaltonViewerMode::Tritanope:
        {
            return true;
        }
        default: return false;
    }
}

std::string daltonViewerModeName (DaltonViewerMode mode)
{
    switch (mode)
    {
        case DaltonViewerMode::None: return "None";
        case DaltonViewerMode::Original: return "Original Image";
        case DaltonViewerMode::HighlightRegions: return "Highlight Same Color";
        case DaltonViewerMode::HSVTransform: return "HSV Manipulation";
        case DaltonViewerMode::Protanope: return "Daltonize - Protanope";
        case DaltonViewerMode::Deuteranope: return "Daltonize - Deuteranope";
        case DaltonViewerMode::Tritanope: return "Daltonize - Tritanope";
        case DaltonViewerMode::FlipRedBlue: return "FlipRedBlue";
        case DaltonViewerMode::FlipRedBlueInvertRed: return "FlipRedBlueInvertRed";
        default: return "Invalid";
    }
}

std::string daltonViewerModeFileName (DaltonViewerMode mode)
{
    switch (mode)
    {
        case DaltonViewerMode::None: return "original";
        case DaltonViewerMode::Original: return "original";
        case DaltonViewerMode::HighlightRegions: return "highlight_similar_colors";
        case DaltonViewerMode::HSVTransform: return "hsv_transform";
        case DaltonViewerMode::Protanope: return "daltonize_protanope";
        case DaltonViewerMode::Deuteranope: return "daltonize_deuteranope";
        case DaltonViewerMode::Tritanope: return "daltonize_tritanope";
        case DaltonViewerMode::FlipRedBlue: return "flip_red_blue";
        case DaltonViewerMode::FlipRedBlueInvertRed: return "flip_red_blue_invert_red";
        default: return "Invalid";
    }
}

struct ImageViewerWindow::Impl
{
    ImguiGLFWWindow imguiGlfwWindow;
    ImageViewerController* controller = nullptr;
    
    ImageViewerWindowState mutableState;

    bool enabled = false;

    ImageCursorOverlay inlineCursorOverlay;
    CursorOverlayInfo cursorOverlayInfo;

    struct {
        bool requested = false;
        std::string outPath;
    } saveToFile;

    struct {
        bool inProgress = false;
        bool needToResize = false;
        int numAlreadyRenderedFrames = 0;
        // This can be higher than 1 on retina displays.
        float screenToImageScale = 1.f;
        dl::Rect targetWindowGeometry;

        void setCompleted () { *this = {}; }
    } updateAfterContentSwitch;
    
    struct {
        Filter_Daltonize daltonize;
        Filter_HSVTransform hsvTransform;
        Filter_FlipRedBlue flipRedBlue;
        Filter_FlipRedBlueAndInvertRed flipRedBlueAndInvertRed;
        Filter_HighlightSimilarColors highlightSimilarColors;
    } filters;
    
    GLFilterProcessor filterProcessor;

    GLTexture gpuTexture;
    dl::ImageSRGBA im;
    std::string imagePath;
    
    ImVec2 monitorSize = ImVec2(-1,-1);
    
    const int windowBorderSize = 0;
    bool shouldUpdateWindowSize = false;
    
    struct {
        dl::Rect normal;
        dl::Rect current;
    } imageWidgetRect;
    
    struct {
        int zoomFactor = 1;
        
        // UV means normalized between 0 and 1.
        ImVec2 uvCenter = ImVec2(0.5f,0.5f);
    } zoom;
    
    void enterMode (DaltonViewerMode newMode)
    {
        this->mutableState.activeMode = newMode;
    }
    
    void advanceMode (bool backwards)
    {
        int newMode_asInt = (int)this->mutableState.activeMode;
        newMode_asInt += backwards ? -1 : 1;
        if (newMode_asInt < 0)
        {
            newMode_asInt = 0;
        }
        else if (newMode_asInt == (int)DaltonViewerMode::NumModes)
        {
            --newMode_asInt;
        }
        
        mutableState.activeMode = (DaltonViewerMode)newMode_asInt;
    }
    
    void onImageWidgetAreaChanged ()
    {
        imguiGlfwWindow.setWindowSize(imageWidgetRect.current.size.x + windowBorderSize * 2,
                                      imageWidgetRect.current.size.y + windowBorderSize * 2);
    }

    GLFilter* filterForMode (DaltonViewerMode modeForCurrentFrame)
    {
        switch (modeForCurrentFrame)
        {
            case DaltonViewerMode::Original:    return nullptr;
            case DaltonViewerMode::HSVTransform: return &filters.hsvTransform;
            case DaltonViewerMode::Protanope:   return &filters.daltonize;
            case DaltonViewerMode::Deuteranope: return &filters.daltonize;
            case DaltonViewerMode::Tritanope:   return &filters.daltonize;
            case DaltonViewerMode::FlipRedBlue: return &filters.flipRedBlue;
            case DaltonViewerMode::FlipRedBlueInvertRed: return &filters.flipRedBlueAndInvertRed;
            
            case DaltonViewerMode::HighlightRegions: 
            {
                if (mutableState.highlightRegion.hasActiveColor())
                    return &filters.highlightSimilarColors;
                return nullptr;
            }

            default: return nullptr;
        }
    }
};

ImageViewerWindow::ImageViewerWindow()
: impl (new Impl())
{}

ImageViewerWindow::~ImageViewerWindow()
{
    shutdown();
}

bool ImageViewerWindow::isEnabled () const
{
    return impl->enabled;
}

void ImageViewerWindow::setEnabled (bool enabled)
{
    if (impl->enabled == enabled)
        return;

    impl->enabled = enabled;

    if (enabled)
    {
        impl->imguiGlfwWindow.setEnabled(true);
    }
    else
    {
        impl->mutableState.activeMode = DaltonViewerMode::None;
        impl->imguiGlfwWindow.setEnabled(false);
    }
}

void ImageViewerWindow::shutdown()
{
    impl->imguiGlfwWindow.shutdown ();
}

bool ImageViewerWindow::initialize (GLFWwindow* parentWindow, ImageViewerController* controller)
{
    impl->controller = controller;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", impl->monitorSize.x, impl->monitorSize.y);

    // Create window with graphics context.
    // glfwWindowHint(GLFW_RESIZABLE, false); // fixed size.

    dl::Rect windowGeometry;
    windowGeometry.origin.x = 0;
    windowGeometry.origin.y = 0;
    windowGeometry.size.x = 640;
    windowGeometry.size.y = 480;
    
    if (!impl->imguiGlfwWindow.initialize (parentWindow, "Dalton Lens Image Viewer", windowGeometry, false /* viewports */))
        return false;

    glfwWindowHint(GLFW_RESIZABLE, true); // restore the default.
    
    // This leads to issues with the window going to the back after a workspace switch.
    // setWindowFlagsToAlwaysShowOnActiveDesktop(impl->imguiGlfwWindow.glfwWindow());
    
    impl->filterProcessor.initializeGL();
    impl->filters.daltonize.initializeGL();
    impl->filters.hsvTransform.initializeGL();
    impl->filters.flipRedBlue.initializeGL();
    impl->filters.flipRedBlueAndInvertRed.initializeGL();
    impl->filters.highlightSimilarColors.initializeGL();

    checkGLError ();

    impl->gpuTexture.initialize();
    
    return true;
}

ImageViewerWindowState& ImageViewerWindow::mutableState ()
{
    return impl->mutableState;
}

void ImageViewerWindow::checkImguiGlobalImageMouseEvents ()
{
    if (viewerModeIsDaltonize(impl->mutableState.activeMode))
    {
        // Handle the mouse wheel to adjust the severity.
        auto& io = ImGui::GetIO ();
        if (io.MouseWheel != 0.f)
        {
#if PLATFORM_MACOS
            const float scaleFactor = 0.5f;
#else
            const float scaleFactor = 0.1f;
#endif
            impl->mutableState.daltonizeSeverity = dl::keepInRange (impl->mutableState.daltonizeSeverity + io.MouseWheel * scaleFactor, 0.f, 1.f);
        }
    }
    else if (impl->mutableState.activeMode == DaltonViewerMode::HSVTransform)
    {
        // Handle the mouse wheel to adjust the severity.
        auto& io = ImGui::GetIO ();
        if (io.MouseWheel != 0.f)
        {
#if PLATFORM_MACOS
            const float scaleFactor = 10.f;
#else
            const float scaleFactor = 2.f;
#endif
            impl->mutableState.hsvTransform.hueShift = dl::keepInRange (impl->mutableState.hsvTransform.hueShift + io.MouseWheel * scaleFactor, 0.f, 359.f);
        }
    }
}

void ImageViewerWindow::checkImguiGlobalImageKeyEvents ()
{
    // These key events are valid also in the control window.
    auto& io = ImGui::GetIO();

    for (const auto code : {GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_S, GLFW_KEY_W, GLFW_KEY_N, GLFW_KEY_A, GLFW_KEY_SPACE})
    {
        if (ImGui::IsKeyPressed(code))
            processKeyEvent(code);
    }

    // Those don't have direct GLFW keycodes for some reason.
    for (const auto c : {'<', '>'})
    {
        if (io.InputQueueCharacters.contains(c))
            processKeyEvent(c);
    }
}

void ImageViewerWindow::processKeyEvent (int keycode)
{
    auto& io = ImGui::GetIO();

    switch (keycode)
    {
        case GLFW_KEY_W:
        case GLFW_KEY_UP: 
        {
            impl->advanceMode(true /* backwards */); 
            break;
        }

        case GLFW_KEY_S:
        {
            if (io.KeyCtrl)
            {
                saveCurrentImage();
            }
            else
            {
                impl->advanceMode(false /* forward */);    
            }
            break;
        }

        case GLFW_KEY_DOWN:
        {
            impl->advanceMode(false /* forward */);
            break;
        }

        case GLFW_KEY_N: 
        {
            impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
            impl->shouldUpdateWindowSize = true;
            break;
        }

        case GLFW_KEY_A:
        {
            float ratioX = impl->imageWidgetRect.current.size.x / impl->imageWidgetRect.normal.size.x;
            float ratioY = impl->imageWidgetRect.current.size.y / impl->imageWidgetRect.normal.size.y;
            if (ratioX < ratioY)
            {
                impl->imageWidgetRect.current.size.y = ratioX * impl->imageWidgetRect.normal.size.y;
            }
            else
            {
                impl->imageWidgetRect.current.size.x = ratioY * impl->imageWidgetRect.normal.size.x;
            }
            impl->shouldUpdateWindowSize = true;
            break;
        }

        case '<':
        {
            if (impl->imageWidgetRect.current.size.x > 64 && impl->imageWidgetRect.current.size.y > 64)
            {
                impl->imageWidgetRect.current.size.x *= 0.5f;
                impl->imageWidgetRect.current.size.y *= 0.5f;
                impl->shouldUpdateWindowSize = true;
            }
            break;
        }

        case '>':
        {
            impl->imageWidgetRect.current.size.x *= 2.f;
            impl->imageWidgetRect.current.size.y *= 2.f;
            impl->shouldUpdateWindowSize = true;
            break;
        }

        case GLFW_KEY_SPACE:
        {
            if (viewerModeIsDaltonize(impl->mutableState.activeMode))
            {
                impl->mutableState.daltonizeShouldSimulateOnly = !impl->mutableState.daltonizeShouldSimulateOnly;
            }
            break;
        }
    }
}

void ImageViewerWindow::saveCurrentImage ()
{
    nfdchar_t *outPath = NULL;
    std::string default_name = formatted("daltonlens_%s.png", daltonViewerModeFileName(impl->mutableState.activeMode).c_str());
    nfdfilteritem_t filterItems[] = { { "Images", "png" } };
    nfdresult_t result = NFD_SaveDialogU8 (&outPath, filterItems, 1, nullptr, default_name.c_str());

    if ( result == NFD_OKAY )
    {
        dl_dbg ("Saving to %s", outPath);
        impl->saveToFile.requested = true;
        impl->saveToFile.outPath = outPath;
        NFD_FreePathU8(outPath);
    }
    else if ( result == NFD_CANCEL ) 
    {
        dl_dbg ("Save image cancelled");
    }
    else 
    {
        fprintf(stderr, "Error: %s\n", NFD_GetError() );
    }
}

dl::Rect ImageViewerWindow::geometry () const
{
    return impl->imguiGlfwWindow.geometry();
}

const CursorOverlayInfo& ImageViewerWindow::cursorOverlayInfo() const
{
    return impl->cursorOverlayInfo;
}

void ImageViewerWindow::showGrabbedData (const GrabScreenData& grabbedData, dl::Rect& updatedWindowGeometry)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);

    impl->im.ensureAllocatedBufferForSize(grabbedData.srgbaImage->width(), grabbedData.srgbaImage->height());
    impl->im.copyDataFrom(*grabbedData.srgbaImage);
    impl->imagePath = "DaltonLens";

    // dl::writePngImage("/tmp/debug.png", impl->im);
    
    impl->imguiGlfwWindow.enableContexts ();
    impl->gpuTexture.upload(impl->im);
    impl->imageWidgetRect.normal.origin = grabbedData.capturedScreenRect.origin;
    impl->imageWidgetRect.normal.size = grabbedData.capturedScreenRect.size;
    impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
    
    impl->mutableState.highlightRegion.setImage(&impl->im);
    impl->mutableState.activeMode = DaltonViewerMode::HighlightRegions;
    
    // Don't show it now, but tell it to show the window after
    // updating the content, otherwise we can get annoying flicker.
    impl->updateAfterContentSwitch.inProgress = true;
    impl->updateAfterContentSwitch.needToResize = true;
    impl->updateAfterContentSwitch.numAlreadyRenderedFrames = 0;
    impl->updateAfterContentSwitch.targetWindowGeometry.origin.x = impl->imageWidgetRect.normal.origin.x - impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.origin.y = impl->imageWidgetRect.normal.origin.y - impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.size.x = impl->imageWidgetRect.normal.size.x + 2 * impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.size.y = impl->imageWidgetRect.normal.size.y + 2 * impl->windowBorderSize;
    impl->updateAfterContentSwitch.screenToImageScale = grabbedData.screenToImageScale;

    updatedWindowGeometry = impl->updateAfterContentSwitch.targetWindowGeometry;
}

void ImageViewerWindow::runOnce ()
{
    if (impl->updateAfterContentSwitch.needToResize)
    {
        impl->imguiGlfwWindow.enableContexts();

        impl->imguiGlfwWindow.setWindowSize (impl->updateAfterContentSwitch.targetWindowGeometry.size.x, 
                                             impl->updateAfterContentSwitch.targetWindowGeometry.size.y);        

        impl->updateAfterContentSwitch.needToResize = false;
    }

    const auto frameInfo = impl->imguiGlfwWindow.beginFrame ();
    const auto& controlsWindowState = impl->controller->controlsWindow()->inputState();

    // Might get filled later on.
    impl->cursorOverlayInfo = {};
    
    // If we do not have a pending resize request, then adjust the content size to the
    // actual window size. The framebuffer might be bigger depending on the retina scale
    // factor.
    if (!impl->shouldUpdateWindowSize)
    {
        impl->imageWidgetRect.current.size.x = frameInfo.windowContentWidth;
        impl->imageWidgetRect.current.size.y = frameInfo.windowContentHeight;
    }

    impl->mutableState.highlightRegion.updateFrameCount ();
   
    auto& io = ImGui::GetIO();    
    
    impl->mutableState.inputState.shiftIsPressed = io.KeyShift;

    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE) || impl->imguiGlfwWindow.closeRequested())
        {
            impl->mutableState.activeMode = DaltonViewerMode::None;
            impl->mutableState.highlightRegion.clearSelection();
            impl->imguiGlfwWindow.cancelCloseRequest ();
        }

        checkImguiGlobalImageKeyEvents ();
    }
    checkImguiGlobalImageMouseEvents ();
    
    impl->mutableState.modeForCurrentFrame = impl->mutableState.activeMode;
    if (impl->mutableState.inputState.shiftIsPressed
        || controlsWindowState.shiftIsPressed)
    {
        impl->mutableState.modeForCurrentFrame = DaltonViewerMode::Original;
    }

    if (impl->shouldUpdateWindowSize)
    {
        impl->onImageWidgetAreaChanged();
        impl->shouldUpdateWindowSize = false;
    }
    
    dl::Rect platformWindowGeometry = impl->imguiGlfwWindow.geometry();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(frameInfo.windowContentWidth, frameInfo.windowContentHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar
                            | ImGuiWindowFlags_NoResize
                            | ImGuiWindowFlags_NoMove
                            | ImGuiWindowFlags_NoScrollbar
                            // ImGuiWindowFlags_NoScrollWithMouse
                            // | ImGuiWindowFlags_NoCollapse
                            | ImGuiWindowFlags_NoBackground
                            | ImGuiWindowFlags_NoSavedSettings
                            | ImGuiWindowFlags_HorizontalScrollbar
                            // | ImGuiWindowFlags_NoDocking
                            | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    bool isOpen = true;
    
    std::string mainWindowName = impl->imagePath + " - " + daltonViewerModeName(impl->mutableState.modeForCurrentFrame);        
    glfwSetWindowTitle(impl->imguiGlfwWindow.glfwWindow(), mainWindowName.c_str());

    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
    {
        if (!isOpen)
        {
            impl->mutableState.activeMode = DaltonViewerMode::None;
        }
                      
        const ImVec2 imageWidgetTopLeft = ImGui::GetCursorScreenPos();
        
        ImVec2 uv0 (0,0);
        ImVec2 uv1 (1.f/impl->zoom.zoomFactor,1.f/impl->zoom.zoomFactor);
        ImVec2 uvRoiCenter = (uv0 + uv1) * 0.5f;
        uv0 += impl->zoom.uvCenter - uvRoiCenter;
        uv1 += impl->zoom.uvCenter - uvRoiCenter;
        
        // Make sure the ROI fits in the image.
        ImVec2 deltaToAdd (0,0);
        if (uv0.x < 0) deltaToAdd.x = -uv0.x;
        if (uv0.y < 0) deltaToAdd.y = -uv0.y;
        if (uv1.x > 1.f) deltaToAdd.x = 1.f-uv1.x;
        if (uv1.y > 1.f) deltaToAdd.y = 1.f-uv1.y;
        uv0 += deltaToAdd;
        uv1 += deltaToAdd;
    
        // Set the params of the active filter.
        switch (impl->mutableState.modeForCurrentFrame)
        {
            case DaltonViewerMode::HighlightRegions: 
            {
                impl->filters.highlightSimilarColors.setParams(impl->mutableState.highlightRegion.mutableData.shaderParams);
                break;
            }

            case DaltonViewerMode::HSVTransform: 
            {
                impl->filters.hsvTransform.setParams(impl->mutableState.hsvTransform);
                break;
            }
            
            case DaltonViewerMode::Protanope:
            case DaltonViewerMode::Deuteranope:
            case DaltonViewerMode::Tritanope:
            {
                Filter_Daltonize::Params params;
                params.kind = static_cast<Filter_Daltonize::Params::Kind>((int)impl->mutableState.modeForCurrentFrame - (int)DaltonViewerMode::Protanope);
                params.simulateOnly = impl->mutableState.daltonizeShouldSimulateOnly;
                params.severity = impl->mutableState.daltonizeSeverity;
                impl->filters.daltonize.setParams (params);
                break;
            }

            default: break;
        }

        GLFilter* activeFilter = impl->filterForMode(impl->mutableState.modeForCurrentFrame);
        GLTexture* imageTexture = &impl->gpuTexture;
        if (activeFilter)
        {
            impl->filterProcessor.render (
                *activeFilter,
                impl->gpuTexture.textureId (),
                impl->gpuTexture.width (),
                impl->gpuTexture.height ()
            );
            imageTexture = &impl->filterProcessor.filteredTexture();
        }

        if (impl->saveToFile.requested)
        {
            impl->saveToFile.requested = false;
            ImageSRGBA im;
            imageTexture->download (im);
            writePngImage (impl->saveToFile.outPath, im);
        }
        
        const bool imageHasNonMultipleSize = int(impl->imageWidgetRect.current.size.x) % int(impl->imageWidgetRect.normal.size.x) != 0;
        const bool hasZoom = impl->zoom.zoomFactor != 1;
        const bool useLinearFiltering = imageHasNonMultipleSize && !hasZoom;
        // Enable it just for that rendering otherwise the pointer overlay will get filtered too.
        if (useLinearFiltering)
        {
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                                                    {
                                                        ImageViewerWindow *that = reinterpret_cast<ImageViewerWindow *>(cmd->UserCallbackData);
                                                        // Doing both since we're not sure which one will be used.
                                                        // Could store it as a member, but well.
                                                        that->impl->gpuTexture.setLinearInterpolationEnabled(true);
                                                        that->impl->filterProcessor.filteredTexture().setLinearInterpolationEnabled(true);
                                                    },
                                                    this);
        }

        const auto imageWidgetSize = imSize(impl->imageWidgetRect.current);
        ImGui::Image(reinterpret_cast<ImTextureID>(imageTexture->textureId()),
                     imageWidgetSize,
                     uv0,
                     uv1);        

        if (useLinearFiltering)
        {
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                                                    {
                                                        ImageViewerWindow *that = reinterpret_cast<ImageViewerWindow *>(cmd->UserCallbackData);
                                                        that->impl->gpuTexture.setLinearInterpolationEnabled(false);
                                                        that->impl->filterProcessor.filteredTexture().setLinearInterpolationEnabled(false);
                                                    },
                                                    this);
        }

        ImVec2 mousePosInImage (0,0);
        ImVec2 mousePosInTexture (0,0);
        {
            // This 0.5 offset is important since the mouse coordinate is an integer.
            // So when we are in the center of a pixel we'll return 0,0 instead of
            // 0.5,0.5.
            ImVec2 widgetPos = (io.MousePos + ImVec2(0.5f,0.5f)) - imageWidgetTopLeft;
            ImVec2 uv_window = widgetPos / imageWidgetSize;
            mousePosInTexture = (uv1-uv0)*uv_window + uv0;
            mousePosInImage = mousePosInTexture * ImVec2(impl->im.width(), impl->im.height());
        }
        
        bool showCursorOverlay = false;
        const bool pointerOverTheImage = ImGui::IsItemHovered() && impl->im.contains(mousePosInImage.x, mousePosInImage.y);
        if (pointerOverTheImage)
        {
            // Unfortunately we can't easily show it in every mode because the image
            // is only in the GPU. We'd need to implement a cache + CPU retrieval.
            bool showBecauseOfHighlightMode = (impl->mutableState.modeForCurrentFrame == DaltonViewerMode::HighlightRegions && !impl->mutableState.highlightRegion.hasActiveColor());
            bool showBecauseOfOriginal = (impl->mutableState.modeForCurrentFrame == DaltonViewerMode::Original);
            showCursorOverlay = (showBecauseOfHighlightMode || showBecauseOfOriginal);
        }
        
        if (showCursorOverlay)
        {
            impl->cursorOverlayInfo.image = &impl->im;
            impl->cursorOverlayInfo.imageTexture = &impl->gpuTexture;
            impl->cursorOverlayInfo.showHelp = false;
            impl->cursorOverlayInfo.imageWidgetSize = imageWidgetSize;
            impl->cursorOverlayInfo.imageWidgetTopLeft = imageWidgetTopLeft;
            impl->cursorOverlayInfo.uvTopLeft = uv0;
            impl->cursorOverlayInfo.uvBottomRight = uv1;
            impl->cursorOverlayInfo.roiWindowSize = ImVec2(15, 15);
            impl->cursorOverlayInfo.mousePos = io.MousePos;
            // Option: show it next to the mouse.
            // impl->inlineCursorOverlay.showTooltip(impl->cursorOverlayInfo);
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && io.KeyCtrl)
        {
            if ((impl->im.width() / float(impl->zoom.zoomFactor)) > 16.f
                 && (impl->im.height() / float(impl->zoom.zoomFactor)) > 16.f)
            {
                impl->zoom.zoomFactor *= 2;
                impl->zoom.uvCenter = mousePosInTexture;
            }
        }
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        {
            if (impl->mutableState.modeForCurrentFrame == DaltonViewerMode::HighlightRegions)
            {
                impl->mutableState.highlightRegion.togglePlotMode();
            }
        }
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            if (io.KeyCtrl)
            {
                if (impl->zoom.zoomFactor >= 2)
                    impl->zoom.zoomFactor /= 2;
            }
            else
            {
                // xv-like controls focus.
                if (impl->controller) impl->controller->onControlsRequested();
            }
        }

        if (impl->mutableState.modeForCurrentFrame == DaltonViewerMode::HighlightRegions)
        {
            // Accept Alt in case the user is still zooming in.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeySuper && !io.KeyShift)
            {
                impl->mutableState.highlightRegion.setSelectedPixel(mousePosInImage.x, mousePosInImage.y, impl->cursorOverlayInfo);
            }
            
            impl->mutableState.highlightRegion.handleInputEvents ();
        }
    }
        
    ImGui::End();
    ImGui::PopStyleVar();
    
    impl->imguiGlfwWindow.endFrame ();
    
    if (impl->updateAfterContentSwitch.inProgress)
    {
        ++impl->updateAfterContentSwitch.numAlreadyRenderedFrames;
        
        if (impl->updateAfterContentSwitch.numAlreadyRenderedFrames >= 2)
        {
            setEnabled(true);
            // Make sure that even if the viewer was already enabled, then we'll focus it.
            glfwFocusWindow(impl->imguiGlfwWindow.glfwWindow());
            impl->imguiGlfwWindow.setWindowPos(impl->updateAfterContentSwitch.targetWindowGeometry.origin.x,
                                               impl->updateAfterContentSwitch.targetWindowGeometry.origin.y);
            impl->updateAfterContentSwitch.setCompleted(); // not really needed, just to be explicit.
        }
    }
    
    // User pressed q, escape or closed the window. We need to do an empty rendering to
    // make sure the platform windows will get hidden and won't stay as ghosts and create
    // flicker when we enable this again.
    if (impl->mutableState.activeMode == DaltonViewerMode::None)
    {
        if (impl->controller) impl->controller->onDismissRequested ();
    }
}

} // dl
