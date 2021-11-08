//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "ImageViewerControlsWindow.h"

#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/ImguiGLFWWindow.h>
#include <DaltonGUI/ImageViewerWindow.h>
#include <DaltonGUI/ImageViewerWindowState.h>
#include <DaltonGUI/GLFWUtils.h>

#include <Dalton/Utils.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"

#include <cstdio>

namespace dl
{

struct ImageViewerControlsWindow::Impl
{
    ImageViewerObserver* observer = nullptr;
    
    // Debatable, but since we don't need polymorphims I've decided to use composition
    // for more flexibility, encapsulation (don't need to expose all the methods)
    // and explicit code.
    ImguiGLFWWindow imguiGlfwWindow;

    struct {
        bool showAfterNextRendering = false;
        bool needRepositioning = false;
        dl::Point targetPosition;

        void setCompleted () { *this = {}; }
    } updateAfterContentSwitch;

    ImVec2 monitorSize = ImVec2(0,0);

    // Tweaked manually by letting ImGui auto-resize the window.
    // 20 vertical pixels per new line.
    ImVec2 windowSizeAtDefaultDpi = ImVec2(364, 382 + 20 + 20);
    ImVec2 windowSizeAtCurrentDpi = ImVec2(-1,-1);
};

ImageViewerControlsWindow::ImageViewerControlsWindow()
: impl (new Impl ())
{}

ImageViewerControlsWindow::~ImageViewerControlsWindow() = default;

void ImageViewerControlsWindow::shutdown() { impl->imguiGlfwWindow.shutdown(); }

void ImageViewerControlsWindow::setEnabled(bool enabled)
{ 
    impl->imguiGlfwWindow.setEnabled(enabled);
    
    if (enabled)
    {
        if (impl->updateAfterContentSwitch.needRepositioning)
        {
            // Needs to be after on Linux.
            impl->imguiGlfwWindow.setWindowPos(impl->updateAfterContentSwitch.targetPosition.x,
                                               impl->updateAfterContentSwitch.targetPosition.y);
        }
        impl->updateAfterContentSwitch.setCompleted ();
    }
}

bool ImageViewerControlsWindow::isEnabled() const { return impl->imguiGlfwWindow.isEnabled(); }

// Warning: may be ignored by some window managers on Linux.. 
// Working hack would be to call 
// sendEventToWM(window, _glfw.x11.NET_ACTIVE_WINDOW, 2, 0, 0, 0, 0);
// in GLFW (notice the 2 instead of 1 in the source code).
// Kwin ignores that otherwise.
void ImageViewerControlsWindow::bringToFront ()
{
    glfw_reliableBringToFront (impl->imguiGlfwWindow.glfwWindow());
}

bool ImageViewerControlsWindow::initialize (GLFWwindow* parentWindow, ImageViewerObserver* observer)
{
    dl_assert (observer, "Cannot be null, we don't check it everywhere.");
    impl->observer = observer;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);

    const dl::Point dpiScale = ImguiGLFWWindow::primaryMonitorContentDpiScale();

    impl->windowSizeAtCurrentDpi = impl->windowSizeAtDefaultDpi;
    impl->windowSizeAtCurrentDpi.x *= dpiScale.x;
    impl->windowSizeAtCurrentDpi.y *= dpiScale.y;

    dl::Rect geometry;
    geometry.size.x = impl->windowSizeAtCurrentDpi.x;
    geometry.size.y = impl->windowSizeAtCurrentDpi.y;
    geometry.origin.x = (impl->monitorSize.x - geometry.size.x)/2;
    geometry.origin.y = (impl->monitorSize.y - geometry.size.y)/2;

    glfwWindowHint(GLFW_RESIZABLE, false); // fixed size.
    bool ok = impl->imguiGlfwWindow.initialize (parentWindow, "DaltonLens Controls", geometry);
    glfwWindowHint(GLFW_RESIZABLE, true); // restore the default.

    // This is tricky, but with GLFW windows we can't have multiple windows waiting for
    // vsync or we'll end up with the framerate being 60 / numberOfWindows. So we'll
    // just keep the image window with the vsync, and skip it for the controls window.
    // Another option would be multi-threading or use a single OpenGL context,
    // but I don't want to introduce that complexity.
    glfwSwapInterval (0);

    return ok;
}

void ImageViewerControlsWindow::repositionAfterNextRendering (const dl::Rect& viewerWindowGeometry, bool showRequested)
{
    // FIXME: padding probably depends on the window manager
    const int expectedHighlightWindowWidthWithPadding = impl->windowSizeAtCurrentDpi.x + 12;
    if (viewerWindowGeometry.origin.x > expectedHighlightWindowWidthWithPadding)
    {
        impl->updateAfterContentSwitch.needRepositioning = true;
        impl->updateAfterContentSwitch.targetPosition.x = viewerWindowGeometry.origin.x - expectedHighlightWindowWidthWithPadding;
        impl->updateAfterContentSwitch.targetPosition.y = viewerWindowGeometry.origin.y;
    }
    else if ((impl->monitorSize.x - viewerWindowGeometry.origin.x - viewerWindowGeometry.size.x) > expectedHighlightWindowWidthWithPadding)
    {
        impl->updateAfterContentSwitch.needRepositioning = true;
        impl->updateAfterContentSwitch.targetPosition.x = viewerWindowGeometry.origin.x + viewerWindowGeometry.size.x + 8;
        impl->updateAfterContentSwitch.targetPosition.y = viewerWindowGeometry.origin.y;
    }
    else
    {
        // Can't fit along side the image window, so just leave it to its default position.
        impl->updateAfterContentSwitch.needRepositioning = false;
    }

    impl->updateAfterContentSwitch.showAfterNextRendering = showRequested;
}

void ImageViewerControlsWindow::runOnce (ImageViewerWindow* activeImageWindow)
{
    const auto frameInfo = impl->imguiGlfwWindow.beginFrame ();

    if (impl->imguiGlfwWindow.closeRequested())
    {
        setEnabled (false);
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        impl->observer->onDismissRequested();
    }

    int menuBarHeight = 0;
    if (ImGui::BeginMainMenuBar())
    {
        menuBarHeight = ImGui::GetWindowSize().y;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save Image", "Ctrl + s", false))
            {
                activeImageWindow->saveCurrentImage ();
            }

            if (ImGui::MenuItem("Close", "q", false))
            {
                impl->observer->onDismissRequested();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filters"))
        {
            if (ImGui::MenuItem("Previous filter", "Up", false)) activeImageWindow->processKeyEvent (GLFW_KEY_UP);
            if (ImGui::MenuItem("Next filter", "Down", false)) activeImageWindow->processKeyEvent (GLFW_KEY_DOWN);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Original size", "n", false)) activeImageWindow->processKeyEvent (GLFW_KEY_N);
            if (ImGui::MenuItem("Double size", ">", false)) activeImageWindow->processKeyEvent ('>');
            if (ImGui::MenuItem("Half size", "<", false)) activeImageWindow->processKeyEvent ('<');
            if (ImGui::MenuItem("Restore aspect ratio", "a", false)) activeImageWindow->processKeyEvent (GLFW_KEY_A);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Help", NULL, false))
                impl->observer->onHelpRequested();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar
                              | ImGuiWindowFlags_NoResize
                              | ImGuiWindowFlags_NoMove
                              | ImGuiWindowFlags_NoScrollbar
                              | ImGuiWindowFlags_NoScrollWithMouse
                              | ImGuiWindowFlags_NoCollapse
                              | ImGuiWindowFlags_NoBackground
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_HorizontalScrollbar
                              | ImGuiWindowFlags_NoDocking
                              | ImGuiWindowFlags_NoNav);

    // Always show the ImGui window filling the GLFW window.
    ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(frameInfo.windowContentWidth, frameInfo.windowContentHeight), ImGuiCond_Always);
    if (ImGui::Begin("DaltonLens Controls", nullptr, flags))
    {
        auto& viewerState = activeImageWindow->mutableState();

        static const char* items[] = {
            "Original Image",
            "Highlight Similar Colors",
            "Daltonize - Protanope",
            "Daltonize - Deuteranope",
            "Daltonize - Tritanope",
            "Flip Red-Blue",
            "Flip Red-Blue and Invert-Red",
        };
        
        int currentItem = int(viewerState.currentMode) + 1;

        ImGui::PushItemWidth(300 * frameInfo.contentDpiScale);
        if (ImGui::Combo("Filter", &currentItem, items, sizeof(items)/sizeof(char*)))
        {
            viewerState.currentMode = DaltonViewerMode(currentItem - 1);
        }
        ImGui::PopItemWidth();

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        static const char* descriptions [] = {
            "Unchanged image content. Useful to inspect the pixel values. You can always temporarily show this by holding the SHIFT key.",
            "Highlight all the pixels that have a color similar to the selected one.",
            "Improve the color contrast for colorblind people with the Daltonize algorithm, optimized for Protanopes.",
            "Improve the color contrast for colorblind people with the Daltonize algorithm, optimized for Deuteranopes.",
            "Improve the color contrast for colorblind people with the Daltonize algorithm, optimized for Tritanopes.",
            "Potentially improve the color contrast by flipping the Red and Blue channels.",
            "Potentially improve the color contrast by flipping the Red and Blue channels and inverting the Red channel.",
        };

        ImGui::Text ("DESCRIPTION");
        ImGui::Bullet(); ImGui::TextWrapped("%s", descriptions[currentItem]);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if (viewerState.currentMode == DaltonViewerMode::HighlightRegions)
        {
            renderHighlightRegionControls(viewerState.highlightRegion, false);
        }

        if (viewerModeIsDaltonize(viewerState.currentMode))
        {
            ImGui::Checkbox("Only simulate color vision deficiency", &viewerState.daltonizeShouldSimulateOnly);
        }

        if (viewerModeIsDaltonize(viewerState.currentMode))
        {
            ImGui::SliderFloat("Severity", &viewerState.daltonizeSeverity, 0.f, 1.f, "%.2f");
        }

        activeImageWindow->checkImguiGlobalImageKeyEvents ();
        activeImageWindow->checkImguiGlobalImageMouseEvents ();

        // Debug: show the FPS.
        if (ImGui::IsKeyPressed(GLFW_KEY_F))
        {
            ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
        }
    }
    ImGui::End();

    // ImGui::ShowDemoWindow();
    // ImGui::ShowUserGuide();
    
    impl->imguiGlfwWindow.endFrame ();
    
    if (impl->updateAfterContentSwitch.showAfterNextRendering)
    {
        setEnabled(true);
    }
}

} // dl
