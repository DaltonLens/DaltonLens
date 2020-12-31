#include "HelpWindow.h"

#include "Graphics.h"
#include "ImGuiUtils.h"

#include "CrossPlatformUtils.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <GLFW/glfw3.h>

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
    bool needToFocusViewportWindowWhenAvailable = false;
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
    impl->needToFocusViewportWindowWhenAvailable = true;
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
    }
}

bool HelpWindow::initialize (GLFWwindow* parentWindow)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);

    glfwWindowHint(GLFW_DECORATED, false);
    impl->window = glfwCreateWindow(1, 1, "DaltonLens Help Fake Window", NULL, parentWindow);
    if (impl->window == NULL)
        return false;
    
    glfwSetWindowPos(impl->window, 0, 0);
    
    glfwMakeContextCurrent(impl->window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl->imGuiContext);
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigWindowsMoveFromTitleBarOnly = false;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    impl->imGuiContext_glfw = ImGui_ImplGlfw_CreateContext();
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    
    impl->imGuiContext_GL3 = ImGui_ImplOpenGL3_CreateContext();
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, false);
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
    
    auto& io = ImGui::GetIO();
    
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();
    
    int display_w, display_h;
    glfwGetFramebufferSize(impl->window, &display_w, &display_h);
    
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        setEnabled(false);
    }

    const float titleBarHeight = ImGui::GetFrameHeight();
    ImGuiWindowFlags flags = (/*ImGuiWindowFlags_NoTitleBar
                               ImGuiWindowFlags_NoResize
                               | ImGuiWindowFlags_NoMove*/
                              // | ImGuiWindowFlags_NoScrollbar
                              ImGuiWindowFlags_NoScrollWithMouse
                              // | ImGuiWindowFlags_NoCollapse
                              | ImGuiWindowFlags_NoBackground
                              | ImGuiWindowFlags_NoSavedSettings
                              | ImGuiWindowFlags_HorizontalScrollbar
                              | ImGuiWindowFlags_NoDocking
                              | ImGuiWindowFlags_NoNav);
    bool isOpen = true;
    
    ImGui::SetNextWindowPos(ImVec2(impl->monitorSize.x/2 - 64, impl->monitorSize.y/2 - 64), ImGuiCond_Once);
    if (ImGui::Begin("DaltonLens Help", &isOpen, flags))
    {
        if (!isOpen)
        {
            setEnabled(false);
        }
        
        // Horrible hack to make sure that our window has the focus once we hide the main window.
        // Otherwise Ctrl+Click might not work right away.
        if (impl->needToFocusViewportWindowWhenAvailable)
        {
            auto* vp = ImGui::GetWindowViewport();
            if (vp->PlatformHandle != nullptr)
            {
                glfwFocusWindow((GLFWwindow*)vp->PlatformHandle);
                impl->needToFocusViewportWindowWhenAvailable = false;
            }
        }
                
        ImGui::Text("GLOBAL COLOR POINTER:");
        ImGui::BulletText("Ctrl+Alt+Cmd+Space to enable the color pointer.");
        ImGui::BulletText("Escape or 'q' to exit.");
        ImGui::BulletText("Click and drag to open a region in the Image Viewer.");
        ImGui::Separator();

        ImGui::Text("IMAGE VIEWER WINDOW:");
        ImGui::BulletText("Escape or 'q' to exit.");
        ImGui::BulletText("Left/Right arrows to switch the mode.");
        ImGui::BulletText("Shift key at any moment to see the original content.");
        ImGui::BulletText("Right click to open the contextual menu.");
        ImGui::BulletText("Available modes:");
        ImGui::Indent();
            ImGui::BulletText("Highlight Similar Colors: click on a pixel to\nhighlight other pixels with a similar color.");
            ImGui::BulletText("Daltonize - Protanope/Deuteranope/Tritanope: \ntransform colors to be more friendly with colorblindness.");
            ImGui::BulletText("Flip Red & Blue (and Invert Red): switch the \ncolor channels to help see color differences.");
        ImGui::Unindent();
        ImGui::Separator();
        
        ImGui::Text("ABOUT DALTONLENS:");
        ImGui::BulletText("Report issues: "); TextURL("https://github.com/DaltonLens/DaltonLens", "https://github.com/DaltonLens/DaltonLens", true, true); ImGui::Text(".");
        ImGui::BulletText("Developed by "); TextURL("Nicolas Burrus", "http://nicolas.burrus.name", true, true);  ImGui::Text(".");
    }
    ImGui::End();
    
    // Rendering
    ImGui::Render();

    //    glViewport(0, 0, display_w, display_h);
    //    glClearColor(0, 0, 0, 1);
    //    glClear(GL_COLOR_BUFFER_BIT);
    //
    //    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
    
    // glfwSwapBuffers(impl->window);
    
    // Just got disabled, render once more.
    if (!impl->enabled)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        
        // Reset the keyboard state to make sure we won't re-enter the next time
        // with 'q' or 'escape' already pressed from before.
        std::fill (io.KeysDown, io.KeysDown + sizeof(io.KeysDown)/sizeof(io.KeysDown[0]), false);
        
        ImGui::NewFrame();
        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }
    
}

} // dl
