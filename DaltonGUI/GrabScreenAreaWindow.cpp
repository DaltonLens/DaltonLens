#include "GrabScreenAreaWindow.h"

#include "Graphics.h"

#include <Dalton/Utils.h>

#include <GLFW/glfw3.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace  dl
{

struct GrabScreenAreaWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    bool enabled = false;
    GLFWwindow* window = nullptr;
};

GrabScreenAreaWindow::GrabScreenAreaWindow()
: impl(new Impl())
{
    
}

GrabScreenAreaWindow::~GrabScreenAreaWindow()
{
    shutdown ();
}

void GrabScreenAreaWindow::shutdown()
{
    if (impl->window)
    {
        ImGui::SetCurrentContext(impl->imGuiContext);
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(impl->imGuiContext);
        impl->imGuiContext = nullptr;
        glfwDestroyWindow(impl->window);
    }
}

bool GrabScreenAreaWindow::initialize (GLFWwindow* parentContext)
{
    // Create window with graphics context.
    // We create a dummy window just because ImGui needs a main window, but we don't really
    // want any, because we prefer to rely on ImGui handling the viewport. This way we can
    // let the ImGui window resizeable, and the platform window will just get resized
    // accordingly. This way we can remove the decorations AND support resize.
    glfwWindowHint(GLFW_DECORATED, false);
    
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);
    
    // Always on top.
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, false);
    
    glfwWindowHint(GLFW_FLOATING, true);
    
    impl->window = glfwCreateWindow(256, 128, "DaltonLens overlay", NULL, parentContext);
    if (impl->window == NULL)
        return false;
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    
    ImGui::StyleColorsDark();
    
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());

    return true;
}

void GrabScreenAreaWindow::runOnce (float mousePosX, float mousePosY, const std::function<unsigned(void)>& allocateTextureUnderCursor)
{
    if (!impl->enabled)
        return;
        
    glfwMakeContextCurrent(impl->window);
    
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto& io = ImGui::GetIO();
    
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
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    bool isOpen = true;
    
    dl_dbg ("Rendering the window.");
    
    glfwSetWindowPos(impl->window, mousePosX + 32, mousePosY + 32);
    
    GLuint underCursorTexture = allocateTextureUnderCursor();
    {
        GLint prevTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        glBindTexture(GL_TEXTURE_2D, underCursorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    std::string mainWindowName = "Invalid";
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(256,128));
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
    {
        if (!isOpen)
        {
            glfwSetWindowShouldClose(impl->window, true);
        }
                       
        if (underCursorTexture > 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(underCursorTexture), ImVec2(64,64));
        }
        
        ImGui::Text("sRGB: [%d %d %d]", 127, 127, 127);
        ImGui::Text("RED");
        
        // ImGuiViewport* vp = ImGui::GetWindowViewport();
        // if (vp && ImGui::GetPlatformIO().Platform_SetWindowFocus)
        // {
        //     ImGui::GetPlatformIO().Platform_SetWindowFocus(vp);
        // }
    }
    ImGui::End();
    // ImGui::PopStyleVar();
        
    // Rendering
    ImGui::Render();
    
    // Not rendering any main window anymore.
    int display_w, display_h;
    glfwGetFramebufferSize(impl->window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glDeleteTextures (1, &underCursorTexture);
    
    glfwSwapBuffers(impl->window);
}

bool GrabScreenAreaWindow::isEnabled () const
{
    return impl->enabled;
}

void GrabScreenAreaWindow::setEnabled (bool enabled)
{
    impl->enabled = enabled;
    
    if (enabled)
    {
        glfwShowWindow(impl->window);
    }
    else
    {
        glfwHideWindow(impl->window);
    }
}

} // dl
