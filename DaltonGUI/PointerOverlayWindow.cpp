#include "PointerOverlayWindow.h"

#include "Graphics.h"
#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>

#include <GLFW/glfw3.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace  dl
{

struct PointerOverlayWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    bool enabled = false;
    bool justGotEnabled = false;
    GLFWwindow* window = nullptr;
    
    ImVec2 monitorSize = ImVec2(-1,-1);
    
    dl::ScreenGrabber pointerScreenGrabber;
};

PointerOverlayWindow::PointerOverlayWindow()
: impl(new Impl())
{
    
}

PointerOverlayWindow::~PointerOverlayWindow()
{
    shutdown ();
}

void PointerOverlayWindow::shutdown()
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

bool PointerOverlayWindow::initialize (GLFWwindow* parentContext)
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
    
    impl->window = glfwCreateWindow(256, 256, "DaltonLens overlay", NULL, parentContext);
    if (impl->window == NULL)
        return false;
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl->imGuiContext);
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
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

void PointerOverlayWindow::runOnce ()
{
    if (!impl->enabled)
        return;
        
    dl_dbg ("Rendering the pointer overlay.");
    
    dl::Point globalMousePos = dl::getMouseCursor();
    globalMousePos.y = impl->monitorSize.y - globalMousePos.y;
    
    glfwMakeContextCurrent(impl->window);
    
    glfwSetWindowPos(impl->window, globalMousePos.x + 32, globalMousePos.y + 32);

    if (impl->justGotEnabled)
    {
        glfwShowWindow(impl->window);
        impl->justGotEnabled = false;
    }
    
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    ImGui::SetCurrentContext(impl->imGuiContext);
    
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
    
    dl::Rect screenRect;
    screenRect.origin.x = globalMousePos.x - 6.5;
    screenRect.origin.y = globalMousePos.y - 6.5;
    screenRect.size.x = 15;
    screenRect.size.y = 15;
    
    // FIXME: only grab the screen if the mouser pointer location has changed, otherwise just reuse the textures.
    dl::ImageSRGBA cpuScreenGrab;
    GLTexture gpuScreenGrab;
    impl->pointerScreenGrabber.grabScreenArea(screenRect,
                                              cpuScreenGrab,
                                              gpuScreenGrab);
       
    GLuint underCursorTexture = gpuScreenGrab.textureId();
    {
        GLint prevTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        glBindTexture(GL_TEXTURE_2D, underCursorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    
    std::string mainWindowName = "Invalid";
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(256,256));
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
    {
        if (!isOpen)
        {
            glfwSetWindowShouldClose(impl->window, true);
        }
                       
        if (underCursorTexture > 0)
        {
            const int zoomLenInPixels = int(screenRect.size.x);
            ImVec2 zoomItemSize (128,128);
            ImVec2 pixelSizeInZoom = zoomItemSize / ImVec2(zoomLenInPixels,zoomLenInPixels);
                       
            ImVec2 zoomTopLeft = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(underCursorTexture), zoomItemSize);
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1 = pixelSizeInZoom * (zoomLenInPixels / 2) + zoomTopLeft;
            ImVec2 p2 = pixelSizeInZoom * ((zoomLenInPixels / 2) + 1) + zoomTopLeft;
            drawList->AddRect(p1, p2, IM_COL32(0,0,0,255));
            drawList->AddRect(p1 - ImVec2(1,1), p2 + ImVec2(1,1), IM_COL32(255,255,255,255));
        }
        
        dl::PixelSRGBA sRgb = cpuScreenGrab(cpuScreenGrab.width()/2, cpuScreenGrab.height()/2);
        
        ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);
        
        float h,s,v;
        ImGui::ColorConvertRGBtoHSV(sRgb.r, sRgb.g, sRgb.b, h, s, v);
        ImGui::Text("HSV: [%.1fº %.1f%% %.1f]", h*360.f, s*100.f, v);
        
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
    
    glfwSwapBuffers(impl->window);
}

bool PointerOverlayWindow::isEnabled () const
{
    return impl->enabled;
}

void PointerOverlayWindow::setEnabled (bool enabled)
{
    if (enabled == impl->enabled)
        return;
    
    impl->enabled = enabled;
    
    if (enabled)
    {
        impl->justGotEnabled = true;
    }
    else
    {
        glfwHideWindow(impl->window);
    }
}

} // dl
