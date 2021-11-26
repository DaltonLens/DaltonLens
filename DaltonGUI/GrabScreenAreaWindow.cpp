//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "GrabScreenAreaWindow.h"

#include "PlatformSpecific.h"

#include <DaltonGUI/ImageCursorOverlay.h>

#include <Dalton/Utils.h>
#include <Dalton/ColorConversion.h>

#include <GLFW/glfw3.h>
#include <clip/clip.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace  dl
{

struct RectSelection
{
    bool isValid () const { return firstCorner.x >= 0.; }
    
    dl::Rect asDL () const
    {
        dl::Rect dlRect;
        dlRect.origin = dl::Point(std::min(firstCorner.x, secondCorner.x), std::min(firstCorner.y, secondCorner.y));
        dlRect.size = dl::Point(std::abs(secondCorner.x - firstCorner.x), std::abs(secondCorner.y - firstCorner.y));
        return dlRect;
    }
    
    ImVec2 firstCorner = ImVec2(-1,-1);
    ImVec2 secondCorner = ImVec2(-1,-1);
};

struct GrabScreenAreaWindow::Impl
{
    enum class State
    {
        Initial,
        MouseDragging
    };
    State currentState = State::Initial;
    
    ImguiGLFWWindow imguiGlfwWindow;

    bool justGotEnabled = false;
    
    bool grabbingFinished = true;
    GrabScreenData grabbedData;
    
    ImVec2 monitorWorkAreaSize = ImVec2(-1,-1);
    ImVec2 monitorWorkAreaTopLeft = ImVec2(-1,-1);
    
    ScreenGrabber grabber;
    
    RectSelection currentSelectionInScreen;

    ImageCursorOverlay cursorOverlay;

    void finishGrabbing ();
};

void GrabScreenAreaWindow::Impl::finishGrabbing ()
{
    this->grabbingFinished = true;
    this->grabbedData.isValid = this->currentSelectionInScreen.isValid();
    if (this->grabbedData.isValid)
    {
        // Initially grabbedData has the entire screen. We're cropping it here.
        
        // Will be higher than 1 on retina displays.
        this->grabbedData.screenToImageScale = this->grabbedData.srgbaImage->width() / this->grabbedData.capturedScreenRect.size.x;
        
        // Cropped screen area.
        dl::Rect croppedScreenRect = this->currentSelectionInScreen.asDL();
        
        dl::Rect croppedImageRect = croppedScreenRect;
        croppedImageRect.origin -= this->grabbedData.capturedScreenRect.origin;
        croppedImageRect *= this->grabbedData.screenToImageScale;
        
        this->grabbedData.capturedScreenRect = croppedScreenRect;

        auto imageRect = dl::Rect::from_x_y_w_h (0, 0, this->grabbedData.srgbaImage->width(), this->grabbedData.srgbaImage->height());

        if (imageRect.intersect (croppedImageRect).area() > 0)
        {
            *this->grabbedData.srgbaImage = dl::crop (*this->grabbedData.srgbaImage, croppedImageRect);
            this->grabbedData.texture.reset(); // not valid anymore.
        }
        else
        {
            // This can happen on Linux if the front most window is the task bar, as
            // it does not overlap with the captured window.
            this->grabbedData.isValid = false;
        }
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    imguiGlfwWindow.setEnabled (false);
}

GrabScreenAreaWindow::GrabScreenAreaWindow()
: impl(new Impl())
{
    
}

GrabScreenAreaWindow::~GrabScreenAreaWindow()
{
    shutdown ();
}

bool GrabScreenAreaWindow::grabbingFinished() const
{
    return impl->grabbingFinished;
}

const GrabScreenData& GrabScreenAreaWindow::grabbedData () const
{
    return impl->grabbedData;
}

void GrabScreenAreaWindow::shutdown()
{
    impl->imguiGlfwWindow.shutdown();
}

bool GrabScreenAreaWindow::initialize (GLFWwindow* parentContext)
{
    dl::Rect geometry;
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        int xpos, ypos, width, height;
        glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &width, &height);
        impl->monitorWorkAreaSize = ImVec2(width, height);
        impl->monitorWorkAreaTopLeft = ImVec2(xpos, ypos);
        geometry.size.x = impl->monitorWorkAreaSize.x;
        geometry.size.y = impl->monitorWorkAreaSize.y;
        geometry.origin.x = impl->monitorWorkAreaTopLeft.x;
        geometry.origin.y = impl->monitorWorkAreaTopLeft.y;
    }

    // Create the window always on top.
    glfwWindowHint(GLFW_DECORATED, false);
    // The transparent framebuffer leads to issues on macOS with the black font.
    // And we don't need it anymore anyway.
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, true);
    glfwWindowHint(GLFW_FLOATING, true);

    if (!impl->imguiGlfwWindow.initialize (parentContext, "DaltonLens Grab Screen", geometry))
    {
        return false;
    }

    // Restore the default settings.
    glfwWindowHint(GLFW_DECORATED, true);
    // glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 0);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, true);
    glfwWindowHint(GLFW_FLOATING, false);    
    
    return true;
}

void GrabScreenAreaWindow::runOnce ()
{
    // Don't try to access the texture data, etc. if it's finished.
    // It could be finished because we're done, or because the capture failed
    // (e.g. screen recording permissions missing).
    if (impl->grabbingFinished)
        return;
    
    const auto frameInfo = impl->imguiGlfwWindow.beginFrame ();

    auto& io = ImGui::GetIO();
    
    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        impl->finishGrabbing ();
        impl->imguiGlfwWindow.endFrame();
        return;
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_SPACE))
    {
        dl::Rect frontWindowRect = dl::getFrontWindowGeometry(impl->imguiGlfwWindow.glfwWindow());
        if (frontWindowRect.size.x >= 0)
        {
            impl->currentSelectionInScreen.firstCorner = imPos(frontWindowRect);
            impl->currentSelectionInScreen.secondCorner = impl->currentSelectionInScreen.firstCorner + imSize(frontWindowRect);
        }
    }
    
    if (ImGui::IsKeyReleased(GLFW_KEY_SPACE))
    {
        impl->finishGrabbing ();
        impl->imguiGlfwWindow.endFrame();
        return;
    }
    
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        impl->currentState = Impl::State::MouseDragging;
        ImVec2 deltaFromInitial = ImGui::GetMouseDragDelta();
        impl->currentSelectionInScreen.firstCorner = io.MousePos - deltaFromInitial + impl->monitorWorkAreaTopLeft;
        impl->currentSelectionInScreen.secondCorner = io.MousePos + impl->monitorWorkAreaTopLeft;
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (impl->currentState == Impl::State::MouseDragging)
        {
            impl->finishGrabbing ();
            impl->imguiGlfwWindow.endFrame();
            return;
        }
    }
    
    ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar
                            | ImGuiWindowFlags_NoResize
                            | ImGuiWindowFlags_NoMove
                            | ImGuiWindowFlags_NoScrollbar
                            | ImGuiWindowFlags_NoScrollWithMouse
                            | ImGuiWindowFlags_NoCollapse
                            // | ImGuiWindowFlags_NoBackground
                            | ImGuiWindowFlags_NoSavedSettings
                            | ImGuiWindowFlags_HorizontalScrollbar
                            | ImGuiWindowFlags_NoDocking
                            | ImGuiWindowFlags_NoNav);
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(255,0,0,127));
    
    std::string mainWindowName = "GrabScreenArea";
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(impl->monitorWorkAreaSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), nullptr, flags))
    {
        {
            auto* drawList = ImGui::GetForegroundDrawList();
            drawList->AddCircleFilled(io.MousePos, 12, IM_COL32(0,0,0,64));
            drawList->AddLine(io.MousePos - ImVec2(16,0), io.MousePos + ImVec2(16,0), IM_COL32(0,0,0,255));
            drawList->AddLine(io.MousePos - ImVec2(0,16), io.MousePos + ImVec2(0,16), IM_COL32(0,0,0,255));
            drawList->AddRectFilled(io.MousePos, io.MousePos + ImVec2(1,1), IM_COL32_WHITE);
        }
        
        const auto imageWidgetTopLeft = ImGui::GetCursorPos();
        const auto imageWidgetSize = impl->monitorWorkAreaSize;
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->grabbedData.texture->textureId()), imageWidgetSize);

        CursorOverlayInfo overlayInfo;
        overlayInfo.image = impl->grabbedData.srgbaImage.get();
        overlayInfo.imageTexture = impl->grabbedData.texture.get();
        overlayInfo.showHelp = true;
        overlayInfo.imageWidgetSize = imageWidgetSize;
        overlayInfo.imageWidgetTopLeft = imageWidgetTopLeft;
        overlayInfo.mousePos = io.MousePos;
        impl->cursorOverlay.showTooltip(overlayInfo);

        ImVec2 selectionFirstCornerInWindow = impl->currentSelectionInScreen.firstCorner - impl->monitorWorkAreaTopLeft;
        ImVec2 selectionSecondCornerInWindow = impl->currentSelectionInScreen.secondCorner - impl->monitorWorkAreaTopLeft;
            
        if (impl->currentSelectionInScreen.isValid())
        {
            auto* drawList = ImGui::GetWindowDrawList();
            
            // Border around the selected area.
            drawList->AddRectFilled(selectionFirstCornerInWindow, selectionSecondCornerInWindow, IM_COL32(0, 0, 127, 64));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    
    ImGui::PopStyleColor();
        
    impl->imguiGlfwWindow.endFrame();
    
    if (impl->justGotEnabled)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        impl->imguiGlfwWindow.setEnabled (true);
        // Not really needed anymore since we just capture the current desktop once.
        // setWindowFlagsToAlwaysShowOnActiveDesktop(impl->imguiGlfwWindow.glfwWindow()); 
        impl->justGotEnabled = false;
    }
}

bool GrabScreenAreaWindow::startGrabbing ()
{
    // Make sure we have the right GL state.
    // FIXME: should we disable it after?
    impl->imguiGlfwWindow.enableContexts ();
    
    impl->currentState = Impl::State::Initial;
    impl->justGotEnabled = true;
    impl->grabbingFinished = false;
    
    impl->currentSelectionInScreen = {};
    
    dl::Rect screenRect;
    screenRect.origin = dl::Point(impl->monitorWorkAreaTopLeft.x, impl->monitorWorkAreaTopLeft.y);
    screenRect.size = dl::Point(impl->monitorWorkAreaSize.x, impl->monitorWorkAreaSize.y);
    
    // Initially we grab the entire screen.
    impl->grabbedData = {};
    impl->grabbedData.srgbaImage = std::make_shared<dl::ImageSRGBA>();
    impl->grabbedData.texture = std::make_shared<GLTexture>();
    impl->grabbedData.capturedScreenRect = screenRect;
    
    bool ok = impl->grabber.grabScreenArea (screenRect, *impl->grabbedData.srgbaImage, *impl->grabbedData.texture);
    if (!ok)
    {
        dl_dbg ("Could not record the screen!");
        // Make it invalid.
        impl->grabbedData = {};
    }
    return ok;
}

bool GrabScreenAreaWindow::isGrabbing() const
{
    return !impl->grabbingFinished;
}

void GrabScreenAreaWindow::dismiss ()
{
    impl->finishGrabbing();
}

} // dl
