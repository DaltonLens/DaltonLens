//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "HelpWindow.h"

#include "Graphics.h"
#include "ImGuiUtils.h"
#include <Dalton/Utils.h>

#include "CrossPlatformUtils.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "GLFWUtils.h"

#include <cstdio>

namespace dl
{

struct HelpWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    ImGui_ImplGlfw_Context* imGuiContext_glfw = nullptr;
    ImGui_ImplOpenGL3_Context* imGuiContext_GL3 = nullptr;

    ImVec2 monitorSize = ImVec2(-1,-1);
    
    GLFWwindow* window = nullptr;
    bool enabled = false;
};

HelpWindow::HelpWindow()
: impl (new Impl())
{}

HelpWindow::~HelpWindow()
{
    shutdown();
}

bool HelpWindow::isEnabled () const
{
    return impl->enabled;
}

void HelpWindow::setEnabled (bool enabled)
{
    if (impl->enabled == enabled)
        return;
    
    impl->enabled = enabled;
    if (impl->enabled)
    {
        GlfwMaintainWindowPosAfterScope _ (impl->window);
        glfwSetWindowShouldClose(impl->window, false);
        glfwShowWindow(impl->window);
    }
    else
    {
        glfwSetWindowShouldClose(impl->window, false);
        glfwHideWindow(impl->window);
    }
}

void HelpWindow::shutdown()
{
    if (impl->window)
    {
        ImGui::SetCurrentContext(impl->imGuiContext);
        ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
        ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
        
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(impl->imGuiContext);
        impl->imGuiContext = nullptr;

        glfwDestroyWindow (impl->window);
        impl->window = nullptr;
    }
}

bool HelpWindow::initialize (GLFWwindow* parentWindow)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);

    // Tweaked manually by letting ImGui auto-resize the window.
    // 20 vertical pixels per new line.
    const int windowWidth = 458;
    const int windowHeight = 348 + 20 + 20;
    
    // glfwWindowHint(GLFW_DECORATED, false);
    impl->window = glfwCreateWindow(windowWidth, windowHeight, "DaltonLens Help", NULL, parentWindow);
    if (impl->window == NULL)
        return false;
    
    setWindowFlagsToAlwaysShowOnActiveDesktop (impl->window);
    
    glfwSetWindowPos(impl->window, (impl->monitorSize.x-windowWidth)/2, (impl->monitorSize.y-windowHeight)/2);
    
    glfwMakeContextCurrent(impl->window);
    
    // Start hidden. setEnabled will show it as needed.
    glfwHideWindow(impl->window);
    
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl->imGuiContext);
    
    impl->imGuiContext_glfw = ImGui_ImplGlfw_CreateContext();
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    
    impl->imGuiContext_GL3 = ImGui_ImplOpenGL3_CreateContext();
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());
    
    return true;
}

void AddUnderLine( ImColor col_ )
{
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    min.y = max.y;
    ImGui::GetWindowDrawList()->AddLine( min, max, col_, 1.0f );
}

// From https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69#file-imguiutils-h-L228-L262
void TextURL( const char* name_, const char* URL_, bool SameLineBefore_, bool SameLineAfter_ )
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
    ImGui::SetCurrentContext(impl->imGuiContext);
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);

    glfwMakeContextCurrent(impl->window);
    
    glfwPollEvents();
    
    int display_w, display_h;
    glfwGetFramebufferSize(impl->window, &display_w, &display_h);
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE) || glfwWindowShouldClose(impl->window))
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
    ImGui::SetNextWindowSize(ImVec2(display_w, display_h), ImGuiCond_Always);
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
        ImGui::BulletText("Left/Right arrows to switch the mode.");
        ImGui::BulletText("Shift key at any moment to see the original content.");
        ImGui::BulletText("Right click to open the contextual menu.");
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
    
    // Rendering
    ImGui::Render();

    glViewport(0, 0, display_w, display_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(impl->window);
}

} // dl
