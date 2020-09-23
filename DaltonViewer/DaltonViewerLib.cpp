#include "DaltonViewerLib.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>

// #include <GL/glew.h>
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#include <GLFW/glfw3.h>

#include <argparse.hpp>

#include <clip/clip.h>

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
        GLint prevTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        
        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, prevTexture);
    }

    void upload (const dl::ImageSRGBA& im)
    {
        GLint prevTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerRow()/im.bytesPerPixel());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im.width(), im.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        
        glBindTexture(GL_TEXTURE_2D, prevTexture);
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

const GLchar *fragmentShader_Normal_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        Out_Color = vec4(srgb.r, srgb.g, srgb.b, srgb.a);
    }
)";

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

    void enable (bool useNearestInterpolation)
    {
        // We need to store the viewport to check its display size later on.
        ImGuiViewport* viewport = ImGui::GetWindowViewport();
        dl_assert (_viewportWhenEnabled == nullptr || viewport == _viewportWhenEnabled, 
                   "You can't enable it multiple times for windows that are not in the same viewport");
        dl_assert (viewport != nullptr, "Invalid viewport.");

        _viewportWhenEnabled = viewport;
        _useNearestInterpolation = useNearestInterpolation;

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
    bool _useNearestInterpolation = true;
};

struct Rect
{
    float x = -1.f;
    float y = -1.f;
    float width = -1.f;
    float height = -1.f;
    
    ImVec2 imPos() const { return ImVec2(x,y); }
    ImVec2 imSize() const { return ImVec2(width,height); }
};

struct DaltonViewer::Impl
{
    bool shouldExit = false;
    GLFWwindow* window = nullptr;
    
    GLTexture gpuTexture;

    std::array<GLShader, 3> shaders;
    int currentShaderIndex = 0;
    GLShader* currentShader = &shaders[0];
    
    dl::ImageSRGBA im;
    std::string imagePath;
    
    ImVec2 monitorSize = ImVec2(-1,-1);
    
    struct {
        Rect normal;
        Rect current;
    } windowSize;
    
    struct {
        int zoomFactor = 1;
        
        // UV means normalized between 0 and 1.
        ImVec2 uvCenter = ImVec2(0.5f,0.5f);
    } zoom;
    
//    ImVec2 uvPointFromMousePos (const ImVec2& mousePos) const
//    {
//
//    }
};

DaltonViewer::DaltonViewer()
: impl (new Impl())
{}

DaltonViewer::~DaltonViewer()
{
    dl_dbg("DaltonViewer::~DaltonViewer");
    
    if (impl->window)
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(impl->window);
        glfwTerminate();
    }
}

bool DaltonViewer::initialize (int argc, char** argv)
{
    dl::ScopeTimer initTimer ("Init");

    for (int i = 0; i < argc; ++i)
    {
        dl_dbg("%d: %s", i, argv[i]);
    }
    
    argparse::ArgumentParser parser("dlv", "0.1");
    parser.add_argument("images")
          .help("Images to visualize")
          .remaining();

    parser.add_argument("--paste")
          .help("Paste the image from clipboard")
          .default_value(false)
          .implicit_value(true);
    
    parser.add_argument("--geometry")
        .help("Geometry of the image area");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error &err)
    {
        std::cerr << "Wrong usage" << std::endl;
        std::cerr << err.what() << std::endl;
        std::cerr << parser;
        return false;
    }

    try
    {
        auto images = parser.get<std::vector<std::string>>("images");
        dl_dbg("%d images provided", (int)images.size());

        impl->imagePath = images[0];
        bool couldLoad = dl::readPngImage(impl->imagePath, impl->im);
        dl_assert (couldLoad, "Could not load the image!");
    }
    catch (std::logic_error& e)
    {
        std::cerr << "No files provided" << std::endl;
    }

    if (parser.present<std::string>("--geometry"))
    {
        std::string geometry = parser.get<std::string>("--geometry");
        int x,y,w,h;
        const int count = sscanf(geometry.c_str(), "%dx%d+%d+%d", &w, &h, &x, &y);
        if (count == 4)
        {
            impl->windowSize.normal.width = w;
            impl->windowSize.normal.height = h;
            impl->windowSize.normal.x = x;
            impl->windowSize.normal.y = y;
        }
        else
        {
            std::cerr << "Invalid geometry string " << geometry << std::endl;
            std::cerr << "Format is WidthxHeight+X+Y" << geometry << std::endl;
            return false;
        }
    }
    
    if (parser.get<bool>("--paste"))
    {
        impl->imagePath = "Pasted from clipboard";

        if (!clip::has(clip::image_format()))
        {
            std::cerr << "Clipboard doesn't contain an image" << std::endl;
            return false;
        }

        clip::image clipImg;
        if (!clip::get_image(clipImg))
        {
            std::cout << "Error getting image from clipboard\n";
            return false;
        }

        clip::image_spec spec = clipImg.spec();

        std::cerr << "Image in clipboard "
            << spec.width << "x" << spec.height
            << " (" << spec.bits_per_pixel << "bpp)\n"
            << "Format:" << "\n"
            << std::hex
            << "  Red   mask: " << spec.red_mask << "\n"
            << "  Green mask: " << spec.green_mask << "\n"
            << "  Blue  mask: " << spec.blue_mask << "\n"
            << "  Alpha mask: " << spec.alpha_mask << "\n"
            << std::dec
            << "  Red   shift: " << spec.red_shift << "\n"
            << "  Green shift: " << spec.green_shift << "\n"
            << "  Blue  shift: " << spec.blue_shift << "\n"
            << "  Alpha shift: " << spec.alpha_shift << "\n";

        switch (spec.bits_per_pixel)
        {
        case 32:
        {
            impl->im.ensureAllocatedBufferForSize((int)spec.width, (int)spec.height);
            impl->im.copyDataFrom((uint8_t*)clipImg.data(), (int)spec.bytes_per_row, (int)spec.width, (int)spec.height);
            break;
        }

        case 16:
        case 24:
        case 64:
        default:
        {
            std::cerr << "Only 32bpp clipboard supported right now." << std::endl;
            return false;
        }
        }
    }

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", impl->monitorSize.x, impl->monitorSize.y);

    if (impl->windowSize.normal.width < 0) impl->windowSize.normal.width = impl->im.width();
    if (impl->windowSize.normal.height < 0) impl->windowSize.normal.height = impl->im.height();
    if (impl->windowSize.normal.x < 0) impl->windowSize.normal.x = impl->monitorSize.x * 0.10;
    if (impl->windowSize.normal.y < 0) impl->windowSize.normal.y = impl->monitorSize.y * 0.10;
    impl->windowSize.current = impl->windowSize.normal;
    
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
    impl->window = glfwCreateWindow(1, 1, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
    if (impl->window == NULL)
        return false;

    glfwSetWindowPos(impl->window, 0, 0);
    
    glfwMakeContextCurrent(impl->window);
    glfwSwapInterval(1); // Enable vsync
    
    // Initialize OpenGL loader
    // bool err = glewInit() != GLEW_OK;
    bool err = gl3wInit() != 0;
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

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
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    impl->gpuTexture.initialize();
    impl->gpuTexture.upload (impl->im);

    impl->shaders[0].initialize(glsl_version, nullptr, fragmentShader_Normal_glsl_130);
    impl->shaders[1].initialize(glsl_version, nullptr, fragmentShader_FlipRedBlue_glsl_130);
    impl->shaders[2].initialize(glsl_version, nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130);
    return true;
}

void DaltonViewer::runOnce ()
{
    if (glfwWindowShouldClose(impl->window))
    {
        impl->shouldExit = true;
        return;
    }
    
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

    // Hack: we need to hide it after fully processing the first ImGui frame, or
    // ImGui will somehow end up showing it again. Also showing it on the very first frame
    // lead to the focus not being given to the app on macOS. So doing it after the second frame.
    if (ImGui::GetFrameCount() == 2)
    {
        dl_dbg ("Hiding the window!");
        glfwHideWindow (impl->window);
        ImGui::SetNextWindowFocus();
    }

    // First condition to update the window is the first time we enter here.
    bool shouldUpdateWindowSize = (ImGui::GetFrameCount() == 1);
    
    auto& io = ImGui::GetIO();
    
    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(GLFW_KEY_Q))
        {
            glfwSetWindowShouldClose(impl->window, true);
        }

        auto updateCurrentShader = [&]() {
            impl->currentShader = &impl->shaders[impl->currentShaderIndex];
        };

        if (ImGui::IsKeyPressed(GLFW_KEY_LEFT))
        {
            --impl->currentShaderIndex;
            if (impl->currentShaderIndex < 0)
            {
                impl->currentShaderIndex = impl->shaders.size()-1;
            }
            updateCurrentShader();
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT))
        {
            ++impl->currentShaderIndex;
            if (impl->currentShaderIndex == impl->shaders.size())
            {
                impl->currentShaderIndex = 0;
            }
            updateCurrentShader ();
        }
    }

    const float titleBarHeight = ImGui::GetFrameHeight();
        
    if (ImGui::IsKeyPressed(GLFW_KEY_N))
    {
        impl->windowSize.current = impl->windowSize.normal;
        shouldUpdateWindowSize = true;
    }
    
    if (ImGui::IsKeyPressed(GLFW_KEY_A))
    {
        float ratioX = impl->windowSize.current.width / impl->windowSize.normal.width;
        float ratioY = impl->windowSize.current.height / impl->windowSize.normal.height;
        if (ratioX < ratioY)
        {
            impl->windowSize.current.height = ratioX * impl->windowSize.normal.height;
        }
        else
        {
            impl->windowSize.current.width = ratioY * impl->windowSize.normal.width;
        }
        shouldUpdateWindowSize = true;
    }
    
    if (io.InputQueueCharacters.contains('<'))
    {
        if (impl->windowSize.current.width > 64 && impl->windowSize.current.height > 64)
        {
            impl->windowSize.current.width *= 0.5f;
            impl->windowSize.current.height *= 0.5f;
            shouldUpdateWindowSize = true;
        }
    }
    
    if (io.InputQueueCharacters.contains('>'))
    {
        impl->windowSize.current.width *= 2.f;
        impl->windowSize.current.height *= 2.f;
        shouldUpdateWindowSize = true;
    }
    
    if (shouldUpdateWindowSize)
    {
        ImGui::SetNextWindowPos(ImVec2(impl->windowSize.current.x, impl->windowSize.current.y - titleBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(impl->windowSize.current.width, impl->windowSize.current.height + titleBarHeight), ImGuiCond_Always);
    }

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
    bool isOpen = true;
    if (ImGui::Begin((impl->imagePath + "###Image").c_str(), &isOpen, flags))
    {
        // Horrible hack to make sure that our window has the focus once we hide the main window.
        // Otherwise Ctrl+Click might not work right away.
        if (ImGui::GetFrameCount()==2)
        {
            auto* vp = ImGui::GetWindowViewport();
            glfwFocusWindow((GLFWwindow*)vp->PlatformHandle);
        }
        
        if (!isOpen)
        {
            glfwSetWindowShouldClose(impl->window, true);
        }
                
        // Make sure we remain up-to-date in case the user resizes it.
        impl->windowSize.current.width = ImGui::GetWindowWidth();
        impl->windowSize.current.height = ImGui::GetWindowHeight() - titleBarHeight;
        impl->windowSize.current.x = ImGui::GetWindowPos().x;
        impl->windowSize.current.y = ImGui::GetWindowPos().y + titleBarHeight;
        
        // ImGuiViewport* vp = ImGui::GetWindowViewport();
        // if (vp && ImGui::GetPlatformIO().Platform_SetWindowFocus)
        // {
        //     ImGui::GetPlatformIO().Platform_SetWindowFocus(vp);
        // }
        
        const auto contentSize = ImGui::GetContentRegionAvail();
        // dl_dbg ("contentSize: %f x %f", contentSize.x, contentSize.y);
        // dl_dbg ("imSize: %f x %f", imSize.x, imSize.y);
        
        impl->currentShader->enable(true);
        
        ImVec2 widgetTopLeft = ImGui::GetCursorScreenPos();
        
        ImVec2 uv0 (0,0);
        ImVec2 uv1 (1.f/impl->zoom.zoomFactor,1.f/impl->zoom.zoomFactor);
        ImVec2 uvRoiCenter = (uv0 + uv1) * 0.5f;
        uv0 += impl->zoom.uvCenter - uvRoiCenter;
        uv1 += impl->zoom.uvCenter - uvRoiCenter;
        
        // Make sure the ROI fits in the image.
        ImVec2 deltaToAdd (0,0);
        if (uv0.x < 0) deltaToAdd.x = -uv0.x;
        if (uv0.y < 0) deltaToAdd.y = -uv0.y;
        if (uv1.x > 1.f) deltaToAdd.x = 1.f-uv1.x;
        if (uv1.y > 1.f) deltaToAdd.y = 1.f-uv1.y;
        uv0 += deltaToAdd;
        uv1 += deltaToAdd;
        
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->gpuTexture.textureId()),
                     impl->windowSize.current.imSize(),
                     uv0,
                     uv1);
        
        ImVec2 mousePosInImage (0,0);
        ImVec2 mousePosInTexture (0,0);
        {
            ImVec2 widgetPos = io.MousePos - widgetTopLeft;
            ImVec2 uv_window = widgetPos / impl->windowSize.current.imSize();
            mousePosInTexture = (uv1-uv0)*uv_window + uv0;
            mousePosInImage = mousePosInTexture * ImVec2(impl->im.width(), impl->im.height());
        }
        
        if (ImGui::IsItemHovered() && io.KeyAlt && impl->im.contains(mousePosInImage.x, mousePosInImage.y))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
            ImGui::BeginTooltip();
            const auto sRgb = impl->im(mousePosInImage.x, mousePosInImage.y);
            ImGui::Text("MousePosInImage: (%d, %d)", (int)mousePosInImage.x, (int)mousePosInImage.y);
            ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
        
//        dl_dbg ("Got click: %d", ImGui::IsItemClicked(ImGuiMouseButton_Left));
//        dl_dbg ("io.KeyCtrl: %d", io.KeyCtrl);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && io.KeyCtrl)
        {
            if ((impl->im.width() / float(impl->zoom.zoomFactor)) > 16.f
                 && (impl->im.height() / float(impl->zoom.zoomFactor)) > 16.f)
            {
                impl->zoom.zoomFactor *= 2;
                impl->zoom.uvCenter = mousePosInTexture;
            }
        }
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && io.KeyCtrl)
        {
            if (impl->zoom.zoomFactor >= 2)
                impl->zoom.zoomFactor /= 2;
        }
        
        if (impl->currentShader)
        {
            impl->currentShader->disable();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // ImGui::ShowDemoWindow();

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

    glfwSwapBuffers(impl->window);
}

bool DaltonViewer::shouldExit() const
{
    return impl->shouldExit;
}
