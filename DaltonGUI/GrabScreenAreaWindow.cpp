//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "GrabScreenAreaWindow.h"

#include "PlatformSpecific.h"

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

        impl->cursorOverlay.showTooltip(*impl->grabbedData.srgbaImage,
                                        *impl->grabbedData.texture,
                                        imageWidgetTopLeft,
                                        imageWidgetSize);

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

namespace dl
{

void ImageCursorOverlay::showTooltip(const dl::ImageSRGBA &image,
                                     GLTexture &imageTexture,
                                     ImVec2 imageWidgetTopLeft,
                                     ImVec2 imageWidgetSize,
                                     const ImVec2 &uvTopLeft,
                                     const ImVec2 &uvBottomRight,
                                     const ImVec2 &roiWindowSize)
{
    auto& io = ImGui::GetIO();
    
    const float monoFontSize = ImguiGLFWWindow::monoFontSize(io);
    const float padding = monoFontSize / 2.f;

    ImVec2 imageSize (image.width(), image.height());
    
    ImVec2 mousePosInImage (0,0);
    ImVec2 mousePosInTexture (0,0);
    {
        // This 0.5 offset is important since the mouse coordinate is an integer.
        // So when we are in the center of a pixel we'll return 0,0 instead of
        // 0.5,0.5.
        ImVec2 widgetPos = (io.MousePos + ImVec2(0.5f,0.5f)) - imageWidgetTopLeft;
        ImVec2 uv_window = widgetPos / imageWidgetSize;
        mousePosInTexture = (uvBottomRight-uvTopLeft)*uv_window + uvTopLeft;
        mousePosInImage = mousePosInTexture * imageSize;
    }
    
    if (!image.contains(mousePosInImage.x, mousePosInImage.y))
        return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding,padding));
    {
        ImGui::BeginTooltip();
        
        const auto sRgb = image((int)mousePosInImage.x, (int)mousePosInImage.y);
        
        const int squareSize = 9*monoFontSize;

        // Show the zoomed image.
        const ImVec2 zoomItemSize (squareSize,squareSize);
        {
            const int zoomLenInPixels = int(roiWindowSize.x);
            ImVec2 pixelSizeInZoom = zoomItemSize / ImVec2(zoomLenInPixels,zoomLenInPixels);
            
            const ImVec2 zoomLen_uv (float(zoomLenInPixels) / image.width(), float(zoomLenInPixels) / image.height());
            ImVec2 zoom_uv0 = mousePosInTexture - zoomLen_uv*0.5f;
            ImVec2 zoom_uv1 = mousePosInTexture + zoomLen_uv*0.5f;
            
            ImVec2 zoomImageTopLeft = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(imageTexture.textureId()), zoomItemSize, zoom_uv0, zoom_uv1);
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1 = pixelSizeInZoom * (zoomLenInPixels / 2) + zoomImageTopLeft;
            ImVec2 p2 = pixelSizeInZoom * ((zoomLenInPixels / 2) + 1) + zoomImageTopLeft;
            drawList->AddRect(p1, p2, IM_COL32(0,0,0,255));
            drawList->AddRect(p1 - ImVec2(1,1), p2 + ImVec2(1,1), IM_COL32(255,255,255,255));
        }
                
        ImGui::SameLine();
        
        float bottomOfSquares = NAN;

        // Show the central pixel color as a filled square
        {
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - ImGui::GetCursorPos();
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 bottomRight = topLeft + ImVec2(squareSize,squareSize);
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32(sRgb.r, sRgb.g, sRgb.b, 255));
            ImGui::SetCursorPosX(topLeft.x + padding);
            ImGui::SetCursorPosY(topLeft.y + padding*2);
            bottomOfSquares = bottomRight.y;
        }
        
        // Show the help
        {
            ImguiGLFWWindow::PushMonoSpaceFont(io);

            ImVec4 color (1, 1, 1, 1);

            const auto hsv = dl::convertToHSV(sRgb);
            
            // White won't be visible on bright colors. This is still not ideal
            // for blue though, where white is more visible than black.
            if (hsv.z > 127)
            {
                color = ImVec4(0.0, 0.0, 0.0, 1.0);
            }

            // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1,0,0,1));
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            auto intRnd = [](float f) { return (int)std::roundf(f); };

            ImGui::BeginChild("ColorInfo", ImVec2(squareSize - padding, zoomItemSize.y));
            ImGui::Text("sRGB %3d %3d %3d", sRgb.r, sRgb.g, sRgb.b);

            PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
            ImGui::Text(" RGB %3d %3d %3d", int(lrgb.r*255.0), int(lrgb.g*255.0), int(lrgb.b*255.0));
            
            ImGui::Text(" HSV %3d %3d %3d", intRnd(hsv.x*360.f), intRnd(hsv.y*100.f), intRnd(hsv.z));
            
            PixelLab lab = dl::convertToLab(sRgb);
            ImGui::Text(" Lab %3d %3d %3d", intRnd(lab.l), intRnd(lab.a), intRnd(lab.b));
            
            PixelXYZ xyz = convertToXYZ(sRgb);
            ImGui::Text(" XYZ %3d %3d %3d", intRnd(xyz.x), intRnd(xyz.y), intRnd(xyz.z));
                        
            ImGui::EndChild();
            ImGui::PopStyleColor();
            // ImGui::PopStyleColor();

            ImGui::PopFont();
        }
        
        auto closestColors = dl::closestColorEntries(sRgb, dl::ColorDistance::CIE2000);
        
        ImGui::SetCursorPosY (bottomOfSquares + padding);

        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(padding*2,padding*2);
            
            // Raw draw is in screen space.
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[0].entry->r, closestColors[0].entry->g, closestColors[0].entry->b, 255));
            
            ImGui::SetCursorPosX(bottomRight.x + padding);
        }

        auto addColorNameAndRGB = [&io](const ColorEntry& entry, const int targetNameSize)
        {
            ImguiGLFWWindow::PushMonoSpaceFont(io);

            auto colorName = formatted("%s / %s", entry.className, entry.colorName);
            if (colorName.size() > targetNameSize)
            {
                colorName = colorName.substr(0, targetNameSize - 3) + "...";
            }
            else if (colorName.size() < targetNameSize)
            {
                colorName += std::string(targetNameSize - colorName.size(), ' ');
            }
            
            ImGui::Text("%s [%3d %3d %3d]",
                        colorName.c_str(),
                        entry.r,
                        entry.g,
                        entry.b);

            ImGui::PopFont();
        };

        addColorNameAndRGB (*closestColors[0].entry, 21);
        
        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(padding*2, padding*2);
            
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[1].entry->r, closestColors[1].entry->g, closestColors[1].entry->b, 255));
            ImGui::SetCursorPosX(bottomRight.x + padding);
        }

        addColorNameAndRGB (*closestColors[1].entry, 21);

        if (!isnan(_timeOfLastCopyToClipboard))
        {
            if (currentDateInSeconds() - _timeOfLastCopyToClipboard < 1.0)
            {
                ImGui::Text ("Copied to clipboard.");
            }
            else
            {
                _timeOfLastCopyToClipboard = NAN;
            }
        }        

        ImGui::EndTooltip();

        if (ImGui::IsKeyPressed(GLFW_KEY_C))
        {
            std::string clipboardText;
            clipboardText += formatted("sRGB %d %d %d\n", sRgb.r, sRgb.g, sRgb.b);
            
            const PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
            clipboardText += formatted("linearRGB %.1f %.1f %.1f\n", lrgb.r, lrgb.g, lrgb.b);

            const auto hsv = dl::convertToHSV(sRgb);
            clipboardText += formatted("HSV %.1fº %.1f%% %.1f\n", hsv.x*360.f, hsv.y*100.f, hsv.z);

            PixelLab lab = dl::convertToLab(sRgb);
            clipboardText += formatted("L*a*b %.1f %.1f %.1f\n", lab.l, lab.a, lab.b);
            
            PixelXYZ xyz = convertToXYZ(sRgb);
            clipboardText += formatted("XYZ %.1f %.1f %.1f\n", xyz.x, xyz.y, xyz.z);

            // glfwSetClipboardString(nullptr, clipboardText.c_str());
            clip::set_text (clipboardText.c_str());
            _timeOfLastCopyToClipboard = currentDateInSeconds();
        }
    }
    ImGui::PopStyleVar();
}

} // dl
