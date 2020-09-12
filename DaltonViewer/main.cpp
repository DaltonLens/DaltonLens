#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>

// #include <GL/glew.h>
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#include <GLFW/glfw3.h>

#include <argparse.hpp>

#include <cstdio>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

class GLTexture
{
public:
    void initialize ()
    {
        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    void upload (const dl::ImageSRGBA& im)
    {
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im.width(), im.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
    }

    GLuint textureId() const { return _textureId; }

private:
    GLuint _textureId = 0;
};

int main(int argc, char** argv)
{
    argparse::ArgumentParser parser("dlv", "0.1");
    parser.add_argument("image")
          .help("Image to visualize");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cout << err.what() << std::endl;
        std::cout << parser;
        exit(1);
    }

    const auto imagePath = parser.get<std::string>("image");

    dl::ImageSRGBA im;
    bool couldLoad = dl::readPngImage(imagePath, im);
    dl_assert (couldLoad, "Could not load the image!");

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    const ImVec2 monitorSize (mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", monitorSize.x, monitorSize.y);

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context.
    // We create a dummy window just because ImGui needs a main window, but we don't really
    // want any, because we prefer to rely on ImGui handling the viewport. This way we can
    // let the ImGui window resizeable, and the platform window will just get resized
    // accordingly. This way we can remove the decorations AND support resize.
    glfwWindowHint(GLFW_DECORATED, false);
    GLFWwindow* window = glfwCreateWindow(16, 16, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
    if (window == NULL)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize OpenGL loader
    // bool err = glewInit() != GLEW_OK;
    bool err = gl3wInit() != 0;
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

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

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    GLTexture gpuTexture;
    gpuTexture.initialize();
    gpuTexture.upload (im);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
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

        // Hack: we need to hide it after processing the first ImGui frame, or
        // ImGui will somehow end up showing it again.
        if (ImGui::GetFrameCount() == 1)
        {
            dl_dbg ("Hiding the window!");
            glfwHideWindow (window);
        }

        if (!io.WantCaptureKeyboard)
        {
            if (ImGui::IsKeyPressed(GLFW_KEY_Q))
            {
                glfwSetWindowShouldClose(window, true);
            }
        }

        const float titleBarHeight = ImGui::GetFrameHeight();        

        const auto imSize = ImVec2(im.width(), im.height());
        const auto windowSize = ImVec2(imSize.x, imSize.y + titleBarHeight);

        auto& mainVP = *ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(monitorSize.x * 0.10, monitorSize.y*0.10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
        ImGuiWindowFlags flags = (/*ImGuiWindowFlags_NoTitleBar*/
                                // ImGuiWindowFlags_NoResize
                                // ImGuiWindowFlags_NoMove
                                // | ImGuiWindowFlags_NoScrollbar
                                ImGuiWindowFlags_NoScrollWithMouse
                                // | ImGuiWindowFlags_NoCollapse
                                | ImGuiWindowFlags_NoBackground
                                | ImGuiWindowFlags_NoSavedSettings
                                | ImGuiWindowFlags_HorizontalScrollbar
                                | ImGuiWindowFlags_NoDocking
                                | ImGuiWindowFlags_NoNav);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::SetNextWindowFocus();
        bool isOpen = true;
        ImGui::Begin((imagePath + "###Image").c_str(), &isOpen, flags);
        if (!isOpen)
        {
            glfwSetWindowShouldClose(window, true);
        }

        // ImGuiViewport* vp = ImGui::GetWindowViewport();
        // if (vp && ImGui::GetPlatformIO().Platform_SetWindowFocus)
        // {
        //     ImGui::GetPlatformIO().Platform_SetWindowFocus(vp);
        // }

        const auto contentSize = ImGui::GetContentRegionAvail();
        // dl_dbg ("contentSize: %f x %f", contentSize.x, contentSize.y);
        // dl_dbg ("imSize: %f x %f", imSize.x, imSize.y);
        ImGui::Image(reinterpret_cast<ImTextureID>(gpuTexture.textureId()), imSize);
        ImGui::End();
        ImGui::PopStyleVar();

        // Rendering
        ImGui::Render();        

        // Not rendering any main window anymore.
        // glViewport(0, 0, display_w, display_h);
        // glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        // glClear(GL_COLOR_BUFFER_BIT);

        // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    	
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

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
