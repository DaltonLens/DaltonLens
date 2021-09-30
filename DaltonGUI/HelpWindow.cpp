//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "HelpWindow.h"

#include <Dalton/OpenGL.h>
#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/ImguiGLFWWindow.h>

#include <Dalton/Utils.h>

#include "PlatformSpecific.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "GLFWUtils.h"

#include <cstdio>

namespace dl
{

bool HelpWindow::initialize (GLFWwindow* parentWindow)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    ImVec2 monitorSize = ImVec2(mode->width, mode->height);

    dl::Rect geometry;
    // Tweaked manually by letting ImGui auto-resize the window.
    // 20 vertical pixels per new line.
    geometry.size.x = 458;
    geometry.size.y = 348 + 20 + 20;
    geometry.origin.x = (monitorSize.x - geometry.size.x)/2;
    geometry.origin.y = (monitorSize.y - geometry.size.y)/2;

    return _imguiGlfwWindow.initialize (parentWindow, "DaltonLens Help", geometry);
}

static void AddUnderLine( ImColor col_ )
{
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    min.y = max.y;
    ImGui::GetWindowDrawList()->AddLine( min, max, col_, 1.0f );
}

// From https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69#file-imguiutils-h-L228-L262
static void TextURL( const char* name_, const char* URL_, bool SameLineBefore_, bool SameLineAfter_ )
{
    if( SameLineBefore_ ){ ImGui::SameLine( 0.0f, ImGui::GetStyle().ItemInnerSpacing.x ); }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    ImGui::Text("%s", name_);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
    {
        if( ImGui::IsMouseClicked(0) )
        {
            dl::openURLInBrowser( URL_ );
        }
        AddUnderLine( ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] );
        // ImGui::SetTooltip( ICON_FA_LINK "  Open in browser\n%s", URL_ );
    }
    else
    {
        AddUnderLine( ImGui::GetStyle().Colors[ImGuiCol_Button] );
    }
    if( SameLineAfter_ ){ ImGui::SameLine( 0.0f, ImGui::GetStyle().ItemInnerSpacing.x ); }
}

void HelpWindow::runOnce ()
{
    const auto frameInfo = _imguiGlfwWindow.beginFrame ();    

    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE) || _imguiGlfwWindow.closeRequested())
    {
        setEnabled(false);
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
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(frameInfo.frameBufferWidth, frameInfo.frameBufferHeight), ImGuiCond_Always);
    if (ImGui::Begin("DaltonLens Help", nullptr, flags))
    {
        // dl_dbg("Window size = %f x %f", ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
        
        ImGui::Text("GLOBAL COLOR POINTER:");
        ImGui::BulletText("Ctrl+Alt+Cmd+Space to enable the color pointer.");
        ImGui::BulletText("Escape or 'q' to exit.");
        ImGui::BulletText("Click and drag to open a region in the Image Viewer.");
        ImGui::BulletText("Space to select the window behind the cursor.");
        ImGui::Separator();

        ImGui::Text("IMAGE VIEWER WINDOW:");
        ImGui::BulletText("Escape or 'q' to exit.");
        ImGui::BulletText("Up/Down arrows to switch the mode.");
        ImGui::BulletText("Shift key at any moment to see the original content.");
        ImGui::BulletText("Right click to show the controls window.");
        ImGui::BulletText("Ctrl + Left/Right click to zoom in or out.");
        ImGui::BulletText("Available modes:");
        ImGui::Indent();
            ImGui::BulletText("Highlight Similar Colors: click on a pixel to\nhighlight other pixels with a similar color.");
            ImGui::BulletText("Daltonize - Protanope/Deuteranope/Tritanope: \ntransform colors to be more friendly with colorblindness.");
            ImGui::BulletText("Flip Red & Blue (and Invert Red): switch the \ncolor channels to help see color differences.");
        ImGui::Unindent();
        ImGui::Separator();
        
        static std::string appVersion;
        static std::string buildNumber;
        if (appVersion.empty())
        {
            getVersionAndBuildNumber(appVersion, buildNumber);
        }
        
        ImGui::Text("ABOUT DALTONLENS:");
        ImGui::BulletText("Dalton Lens %s (build %s)", appVersion.c_str(), buildNumber.c_str());
        ImGui::BulletText("Report issues: "); TextURL("https://github.com/DaltonLens/DaltonLens", "https://github.com/DaltonLens/DaltonLens", true, true); ImGui::Text(".");
        ImGui::BulletText("Developed by "); TextURL("Nicolas Burrus", "http://nicolas.burrus.name", true, true);  ImGui::Text(".");
    }
    ImGui::End();
    
    _imguiGlfwWindow.endFrame ();
}

} // dl
