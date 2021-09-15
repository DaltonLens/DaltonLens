//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "ImguiGLFWWindow.h"

#include "Graphics.h"
#include "ImguiUtils.h"
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
struct ImguiGLFWWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    ImGui_ImplGlfw_Context* imGuiContext_glfw = nullptr;
    ImGui_ImplOpenGL3_Context* imGuiContext_GL3 = nullptr;

    GLFWwindow* window = nullptr;
    bool enabled = false;

    ImguiGLFWWindow::FrameInfo currentFrameInfo;
    
    dl::Point posToSetForNextShow;
    
    std::string title;
};

ImguiGLFWWindow::ImguiGLFWWindow()
: impl (new Impl())
{}

ImguiGLFWWindow::~ImguiGLFWWindow()
{
    shutdown();
}

GLFWwindow* ImguiGLFWWindow::glfwWindow ()
{
    return impl->window;
}

bool ImguiGLFWWindow::isEnabled () const
{
    return impl->enabled;
}

void ImguiGLFWWindow::setEnabled (bool enabled)
{
    if (impl->enabled == enabled)
        return;
    
    impl->enabled = enabled;
    if (impl->enabled)
    {
        glfwSetWindowShouldClose(impl->window, false);
        glfwShowWindow(impl->window);

        // Save the window position as the next show will put it anywhere on Linux :(
        if (impl->posToSetForNextShow.isValid())
        {
            glfwSetWindowPos (impl->window, impl->posToSetForNextShow.x, impl->posToSetForNextShow.y);
            impl->posToSetForNextShow = {};
        }
    }
    else
    {
        // Save the position before the hide.
        int x, y;
        glfwGetWindowPos(impl->window, &x, &y);
        impl->posToSetForNextShow.x = x;
        impl->posToSetForNextShow.y = y;
        glfwSetWindowShouldClose(impl->window, false);
        glfwHideWindow(impl->window);
    }
}

bool ImguiGLFWWindow::closeRequested () const
{
    return glfwWindowShouldClose (impl->window);
}

void ImguiGLFWWindow::cancelCloseRequest ()
{
    glfwSetWindowShouldClose (impl->window, false);
}

void ImguiGLFWWindow::triggerCloseRequest ()
{
    glfwSetWindowShouldClose (impl->window, true);
}

void ImguiGLFWWindow::setWindowPos (int x, int y)
{
    glfwSetWindowPos (impl->window, x, y);
}

void ImguiGLFWWindow::setWindowSize (int width, int height)
{
    glfwSetWindowSize (impl->window, width, height);
}

dl::Rect ImguiGLFWWindow::geometry() const
{
    dl::Rect geom;

    int platformWindowX, platformWindowY;
    glfwGetWindowPos(impl->window, &platformWindowX, &platformWindowY);
    
    int platformWindowWidth, platformWindowHeight;
    glfwGetWindowSize(impl->window, &platformWindowWidth, &platformWindowHeight);

    geom.origin.x = platformWindowX;
    geom.origin.y = platformWindowY;
    
    geom.size.x = platformWindowWidth;
    geom.size.y = platformWindowHeight;
    return geom;
}

void ImguiGLFWWindow::shutdown()
{
    if (impl->window)
    {
        enableContexts ();
        
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(impl->imGuiContext);
        impl->imGuiContext = nullptr;

        glfwDestroyWindow (impl->window);
        impl->window = nullptr;

        disableContexts ();
    }
}

static void glfwErrorFunction (int code, const char* error)
{
    fprintf (stderr, "GLFW Error %d: %s", code, error);
}

static void windowPosCallback(GLFWwindow* w, int x, int y)
{
    dl_dbg ("Got a window pos callback (%p) %d %d", w, x, y);
}

bool ImguiGLFWWindow::initialize (GLFWwindow* parentWindow,
                                  const std::string& title,
                                  const dl::Rect& geometry,
                                  bool enableImguiViewports)
{
    glfwSetErrorCallback (glfwErrorFunction);
    
    impl->title = title;

    // glfwWindowHint(GLFW_DECORATED, false);
    impl->window = glfwCreateWindow(geometry.size.x, geometry.size.y, title.c_str(), NULL, parentWindow);
    if (impl->window == NULL)
        return false;
    
    setWindowFlagsToAlwaysShowOnActiveDesktop (impl->window);
    
    glfwSetWindowPos(impl->window, geometry.origin.x, geometry.origin.y);

    // glfwSetWindowUserPointer(impl->window, this);
    // glfwSetWindowPosCallback(impl->window, [](GLFWwindow* w, int x, int y) { 
    //     reinterpret_cast<ImGuiGLFWWindow*>(glfwGetWindowUserPointer(w))->impl->windowPosCallback (w, h)
    // });

    glfwMakeContextCurrent(impl->window);
    
    // Start hidden. setEnabled will show it as needed.
    glfwHideWindow(impl->window);
    
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl->imGuiContext);
    
    // FIXME: temporarily keeping the viewport setting.
    if (enableImguiViewports)
    {
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        // Enable Multi-Viewport / Platform Windows. Will be used by the highlight similar color companion window.
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    impl->imGuiContext_glfw = ImGui_ImplGlfw_CreateContext();
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    
    impl->imGuiContext_GL3 = ImGui_ImplOpenGL3_CreateContext();
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());
    
    return true;
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

void ImguiGLFWWindow::enableContexts ()
{
    ImGui::SetCurrentContext(impl->imGuiContext);
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    glfwMakeContextCurrent(impl->window);
}

void ImguiGLFWWindow::disableContexts ()
{
    ImGui::SetCurrentContext(nullptr);
    ImGui_ImplGlfw_SetCurrentContext(nullptr);
    ImGui_ImplOpenGL3_SetCurrentContext(nullptr);
}

ImguiGLFWWindow::FrameInfo ImguiGLFWWindow::beginFrame ()
{
    enableContexts ();

    glfwGetFramebufferSize(impl->window, &(impl->currentFrameInfo.frameBufferWidth), &(impl->currentFrameInfo.frameBufferHeight));
    
    glfwPollEvents();
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    return impl->currentFrameInfo;
}

void ImguiGLFWWindow::endFrame ()
{
    // Rendering
    ImGui::Render();

    glViewport(0, 0, impl->currentFrameInfo.frameBufferWidth, impl->currentFrameInfo.frameBufferHeight);
    glClearColor(0.1, 0.1, 0.1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Update and Render additional Platform Windows
    // This is used by the highlight similar color companion window.
    ImGuiIO& io = ImGui::GetIO();
    {
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    glfwSwapBuffers(impl->window);

    // would be safer to call disableContexts now?
}

} // dl
