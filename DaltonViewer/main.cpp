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

static void checkGLError ()
{
    GLenum err = glGetError();
    dl_assert (err == GL_NO_ERROR, "GL Error! 0x%x", (int)err);
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

bool gl_checkProgram(GLint handle, const char* desc, const char* glsl_version)
{
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: GLShader: failed to link %s! (with GLSL '%s')\n", desc, glsl_version);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, NULL, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

bool gl_checkShader(GLuint handle, const char *desc)
{
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: GLShader: failed to compile %s!\n", desc);
    if (log_length > 1)
    {
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, NULL, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

const GLchar *fragmentShader_FlipRedBlue_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        Out_Color = vec4(srgb.b, srgb.g, srgb.r, srgb.a);
    }
)";

const GLchar *fragmentShader_FlipRedBlue_InvertRed_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        Out_Color = vec4(1.0-srgb.b, srgb.g, srgb.r, srgb.a);
    }
)";

class GLShader
{
public:
    void initialize(const char* glslVersionString, const GLchar* vertexShader, const GLchar* fragmentShader)
    {
        const GLchar *defaultVertexShader_glsl_130 =
            "uniform mat4 ProjMtx;\n"
            "in vec2 Position;\n"
            "in vec2 UV;\n"
            "in vec4 Color;\n"
            "out vec2 Frag_UV;\n"
            "out vec4 Frag_Color;\n"
            "void main()\n"
            "{\n"
            "    Frag_UV = UV;\n"
            "    Frag_Color = Color;\n"
            "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
            "}\n";

        const GLchar *defaultFragmentShader_glsl_130 =
            "uniform sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "in vec4 Frag_Color;\n"
            "out vec4 Out_Color;\n"
            "void main()\n"
            "{\n"
            "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
            "}\n";

        // Select shaders matching our GLSL versions
        if (vertexShader == nullptr)
            vertexShader = defaultVertexShader_glsl_130;

        if (fragmentShader == nullptr)
            fragmentShader = defaultFragmentShader_glsl_130;

        // Create shaders
        const GLchar *vertex_shader_with_version[3] = {glslVersionString, "\n", vertexShader};
        _vertHandle = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(_vertHandle, 3, vertex_shader_with_version, NULL);
        glCompileShader(_vertHandle);
        gl_checkShader(_vertHandle, "vertex shader");

        const GLchar *fragment_shader_with_version[3] = {glslVersionString, "\n", fragmentShader};
        _fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(_fragHandle, 3, fragment_shader_with_version, NULL);
        glCompileShader(_fragHandle);
        gl_checkShader(_fragHandle, "fragment shader");

        _shaderHandle = glCreateProgram();
        glAttachShader(_shaderHandle, _vertHandle);
        glAttachShader(_shaderHandle, _fragHandle);
        glLinkProgram(_shaderHandle);
        gl_checkProgram(_shaderHandle, "shader program", glslVersionString);

        _attribLocationTex = glGetUniformLocation(_shaderHandle, "Texture");
        _attribLocationProjMtx = glGetUniformLocation(_shaderHandle, "ProjMtx");
        _attribLocationVtxPos = (GLuint)glGetAttribLocation(_shaderHandle, "Position");
        _attribLocationVtxUV = (GLuint)glGetAttribLocation(_shaderHandle, "UV");
        _attribLocationVtxColor = (GLuint)glGetAttribLocation(_shaderHandle, "Color");

        checkGLError();
    }

    void enable ()
    {
        // We need to store the viewport to check its display size later on.
        ImGuiViewport* viewport = ImGui::GetWindowViewport();
        dl_assert (_viewportWhenEnabled == nullptr || viewport == _viewportWhenEnabled, 
                   "You can't enable it multiple times for windows that are not in the same viewport");
        dl_assert (viewport != nullptr, "Invalid viewport.");

        _viewportWhenEnabled = viewport; 

        auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd) 
        {
            GLShader* that = reinterpret_cast<GLShader*>(cmd->UserCallbackData);
            dl_assert (that, "Invalid user data");
            that->onRenderEnable (parent_list, cmd);
        };

        ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
    }

    void disable ()
    {
        auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd) 
        {
            GLShader* that = reinterpret_cast<GLShader*>(cmd->UserCallbackData);
            dl_assert (that, "Invalid user data");
            that->onRenderDisable (parent_list, cmd);
        };

        ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
    }

private:
    void onRenderEnable (const ImDrawList *parent_list, const ImDrawCmd *drawCmd)
    {
        auto* drawData = _viewportWhenEnabled->DrawData;
        float L = drawData->DisplayPos.x;
        float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
        float T = drawData->DisplayPos.y;
        float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

        const float ortho_projection[4][4] =
        {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };

        glGetIntegerv(GL_CURRENT_PROGRAM, &_prevShaderHandle);
        glUseProgram (_shaderHandle);

        glUniformMatrix4fv(_attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
        glUniform1i(_attribLocationTex, 0);

        checkGLError();
    }

    void onRenderDisable (const ImDrawList *parent_list, const ImDrawCmd *drawCmd)
    {
        glUseProgram (_prevShaderHandle);
        _prevShaderHandle = 0;
        _viewportWhenEnabled = nullptr;
    }

private:
    GLuint _shaderHandle = 0;
    GLuint _vertHandle = 0;
    GLuint _fragHandle = 0;
    GLuint _attribLocationTex = 0;
    GLuint _attribLocationProjMtx = 0;
    GLuint _attribLocationVtxPos = 0;
    GLuint _attribLocationVtxUV = 0;
    GLuint _attribLocationVtxColor = 0;

    GLint _prevShaderHandle = 0;

    ImGuiViewport* _viewportWhenEnabled = nullptr;
};

int main(int argc, char** argv)
{
    dl::ScopeTimer initTimer ("Init");

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

    std::array<GLShader, 2> shaders;
    shaders[0].initialize(glsl_version, nullptr, fragmentShader_FlipRedBlue_glsl_130);
    shaders[1].initialize(glsl_version, nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130);
    int currentShaderIndex = -1;
    GLShader* currentShader = nullptr;

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

            auto updateCurrentShader = [&]() {
                if (currentShaderIndex >= 0)
                {
                    currentShader = &shaders[currentShaderIndex];
                }
                else
                {
                    currentShader = nullptr;
                }
            };

            if (ImGui::IsKeyPressed(GLFW_KEY_LEFT))
            {
                --currentShaderIndex;
                if (currentShaderIndex < -1)
                {
                    currentShaderIndex = shaders.size()-1;
                }
                updateCurrentShader();
            }

            if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT))
            {
                ++currentShaderIndex;
                if (currentShaderIndex == shaders.size())
                {
                    currentShaderIndex = -1;
                }
                updateCurrentShader ();
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

        if (currentShader)
        {
            currentShader->enable();
        }

        ImGui::Image(reinterpret_cast<ImTextureID>(gpuTexture.textureId()), imSize);

        if (currentShader)
        {
            currentShader->disable();
        }
        
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

        initTimer.stop();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
