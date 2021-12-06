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
#include <DaltonGUI/ImageCursorOverlay.h>
#include <DaltonGUI/PlatformSpecific.h>

#include <Dalton/Utils.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"

#include <cstdio>

namespace dl
{

struct ImageViewerControlsWindow::Impl
{
    ImageViewerController* controller = nullptr;
    
    ControlsWindowInputState inputState;

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
    ImVec2 windowSizeAtDefaultDpi = ImVec2(348, 382 + 20 + 20);
    ImVec2 windowSizeAtCurrentDpi = ImVec2(-1,-1);
    
    ImageCursorOverlay cursorOverlay;
};

ImageViewerControlsWindow::ImageViewerControlsWindow()
: impl (new Impl ())
{}

ImageViewerControlsWindow::~ImageViewerControlsWindow() = default;

const ControlsWindowInputState& ImageViewerControlsWindow::inputState () const
{
    return impl->inputState;
}

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
    else
    {
        // Make sure to reset the input state when the window gets dismissed.
        impl->inputState = {};
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

bool ImageViewerControlsWindow::initialize (GLFWwindow* parentWindow, ImageViewerController* controller)
{
    dl_assert (controller, "Cannot be null, we don't check it everywhere.");
    impl->controller = controller;

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
    if (!ok)
    {
        return false;
    }
    
    glfwWindowHint(GLFW_RESIZABLE, true); // restore the default.

    // This leads to issues with the window going to the back after a workspace switch.
    // setWindowFlagsToAlwaysShowOnActiveDesktop(impl->imguiGlfwWindow.glfwWindow());
    
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
    // Try to put it on the left first.
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

void ImageViewerControlsWindow::runOnce ()
{
    const auto frameInfo = impl->imguiGlfwWindow.beginFrame ();
    const auto& io = ImGui::GetIO();
    const float monoFontSize = ImguiGLFWWindow::monoFontSize(io);
    auto* activeImageWindow = impl->controller->activeViewerWindow();

    if (impl->imguiGlfwWindow.closeRequested())
    {
        setEnabled (false);
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        impl->controller->onDismissRequested();
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
                impl->controller->onDismissRequested();
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
                impl->controller->onHelpRequested();
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

        struct FilterEntry {
            const char* itemName;
            const char* description;
            const char* help;
        };

        static const FilterEntry filters[] = {
            { "Original Image",
              "Unchanged image content. Useful to inspect the pixel values. You can always temporarily show this by holding the SHIFT key.",
              nullptr,
            },

            { "Highlight Similar Colors",
              "Highlight all the pixels with a similar color in the image.",
              "The similarity is computed by comparing the HSV values. Hue is weighted more heavily "
              "as anti-aliasing can change the saturation and value significantly.\n\n"
              "Shortcuts\n"
              "  space: toggle the plot mode\n"
              "  mouse wheel: adjust the threshold\n"
              "  hold shift: hold to show the original image"
            },

            { "Daltonize",
              "Improve the color contrast for colorblind people with the Daltonize algorithm.",
              "This implements the Daltonize algorithm by Fidaner et al. \"Analysis of Color Blindness\" (2005).\n\n"
                "Use Left-Right arrows to switch the deficiency kind, and the mouse wheel to adjust the severity."
            },

            { "HSV Transform",
              "Manipulate Hue and Saturation to better discriminate some colors.",
              "Shifting the Hue can help make the colors easier to differentiate. Increasing the saturation can also "
              "help make the colors more vivid, and hue quantization can \"simplify\" the colors into fewer obvious groups."
            },

            { "Flip Red-Blue",
              "Potentially improve the color contrast by flipping the Red and Blue channels.",
              "The transform is applied in the YCbCr color space and flips the cb and cr components."
            },

            { "Flip Red-Blue and Invert-Red",
              "Potentially improve the color contrast by flipping the Red and Blue channels and inverting the Red channel.",
              "The transform is applied in the YCbCr color space and flips the cb and cr components, after negating cb."
            },
        };
        
        int itemForThisFrame = std::max(int(viewerState.modeForCurrentFrame) + 1, 0);

        ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth() - 2*monoFontSize);
        if (ImGui::BeginCombo("", filters[itemForThisFrame].itemName, ImGuiComboFlags_None))
        {
            for (int i = 0; i < IM_ARRAYSIZE(filters); ++i)
            {
                const auto& item = filters[i];
                const bool is_selected = (itemForThisFrame == i);
                if (ImGui::Selectable(item.itemName, is_selected))
                {
                    itemForThisFrame = i;
                    viewerState.activeMode = DaltonViewerMode(itemForThisFrame - 1);
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (filters[itemForThisFrame].help)
        {
            helpMarker (filters[itemForThisFrame].help, monoFontSize * 19);
        }
        
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();        

        ImGui::TextWrapped("%s", filters[itemForThisFrame].description);

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        const auto* cursorOverlayInfo = &activeImageWindow->cursorOverlayInfo();

        if (viewerState.modeForCurrentFrame == DaltonViewerMode::HighlightRegions)
        {
            renderHighlightRegionControls(viewerState.highlightRegion, false);
            if (viewerState.highlightRegion.hasActiveColor())
            {
                cursorOverlayInfo = &viewerState.highlightRegion.mutableData.cursorOverlayInfo;
            }
        }

        if (viewerModeIsDaltonize(viewerState.modeForCurrentFrame))
        {
            static const char* items[] = {
                "Protanope (red-blind)",
                "Deuteranope (green-blind)",
                "Tritanope (blue-blind)",
            };
            // IMGUI_API bool          Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1);
            ImGui::Combo("Deficiency Type", 
                reinterpret_cast<int*>(&viewerState.daltonizeParams.kind),
                items,
                3
            );
            ImGui::Checkbox("Only simulate color vision deficiency", &viewerState.daltonizeParams.simulateOnly);
            ImGui::SliderFloat("Severity", &viewerState.daltonizeParams.severity, 0.f, 1.f, "%.2f");
        }

        if (viewerState.modeForCurrentFrame == DaltonViewerMode::HSVTransform)
        {
            ImGui::SliderInt("Hue Shift", &viewerState.hsvTransform.hueShift, 0, 359, "%dº");
            ImGui::SliderFloat("Saturation Boost", &viewerState.hsvTransform.saturationScale, 1.f, 8.f, "%.1f");
            ImGui::SliderInt("Hue Quantization", &viewerState.hsvTransform.hueQuantization, 1, 100, "%d");
        }
        
        if (cursorOverlayInfo->valid())
        {
            ImGui::SetCursorPosY (ImGui::GetWindowHeight() - monoFontSize*15.5);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1,0,0,1));
            ImGui::BeginChild("CursorOverlay", ImVec2(monoFontSize*21, monoFontSize*14), false /* no border */, windowFlagsWithoutAnything());
            impl->cursorOverlay.showTooltip(*cursorOverlayInfo, false /* not as tooltip */);
            ImGui::EndChild();
            // ImGui::PopStyleColor();
        }
        
        activeImageWindow->checkImguiGlobalImageKeyEvents ();
        activeImageWindow->checkImguiGlobalImageMouseEvents ();
        
        // Debug: show the FPS.
        if (ImGui::IsKeyPressed(GLFW_KEY_F))
        {
            ImGui::Text("%.1f FPS", io.Framerate);
        }

        impl->inputState.shiftIsPressed = io.KeyShift;
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
