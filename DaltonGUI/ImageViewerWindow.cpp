#include "ImageViewerWindow.h"

#include "Graphics.h"
#include "ImGuiUtils.h"

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/ColorConversion.h>

#include <GLFW/glfw3.h>

#include <argparse.hpp>

#include <clip/clip.h>

#include <cstdio>

namespace dl
{

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
        vec3 yCbCr = yCbCrFromSRGBA(srgb);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = yCbCr.z;
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGBAfromYCbCr (transformedYCbCr, 1.0);
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
        vec3 yCbCr = yCbCrFromSRGBA(srgb);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = -yCbCr.z; // flip Cb
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGBAfromYCbCr (transformedYCbCr, 1.0);
    }
)";

const GLchar *fragmentShader_DaltonizeV1_Protanope_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 lms = lmsFromSRGBA(srgba);
        vec3 lmsSimulated = applyProtanope(lms);
        vec4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
        vec4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
        Out_Color = srgbaOut;
    }
)";

const GLchar *fragmentShader_DaltonizeV1_Deuteranope_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 lms = lmsFromSRGBA(srgba);
        vec3 lmsSimulated = applyDeuteranope(lms);
        vec4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
        vec4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
        Out_Color = srgbaOut;
    }
)";

const GLchar *fragmentShader_DaltonizeV1_Tritanope_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 lms = lmsFromSRGBA(srgba);
        vec3 lmsSimulated = applyTritanope(lms);
        vec4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
        vec4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
        Out_Color = srgbaOut;
    }
)";


enum class DaltonViewerMode {
    None = -1,
    Main = 0,
    HighlightRegions,
};

struct HighlightRegion
{
    void initialize (const char* glslVersion, dl::ImageSRGBA& im)
    {
        _im = &im;
        
        const GLchar *fragmentShader_highlightSameColor = R"(
            uniform sampler2D Texture;
            uniform vec3 u_refColor;
            uniform float u_distThreshold;
            uniform int u_frameCount;
            in vec2 Frag_UV;
            in vec4 Frag_Color;
            out vec4 Out_Color;
        
            float hsvDist_AA(vec3 hsv1, vec3 hsv2)
            {
                // hue is modulo 1.0 (360 degrees)
                float dh = abs(hsv1.x - hsv2.x);
                float h_dist = min (dh, 1.0-dh);
                float s_dist = abs(hsv1.y - hsv2.y) * 0.1;
                float v_dist = abs(hsv1.z - hsv2.z) * 0.05;
                return 255.0 * sqrt(h_dist*h_dist + s_dist*s_dist + v_dist*v_dist);
            }
        
            void main()
            {
                vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
                vec3 hsv = HSVFromSRGB(srgba.rgb);                
                vec3 ref_hsv = HSVFromSRGB(u_refColor.rgb);
                
                // vec3 delta = abs(srgba.rgb - u_refColor.rgb);
                // float maxDelta = max(delta.r, max(delta.g, delta.b));
                float dist = hsvDist_AA(ref_hsv, hsv);
                float isSame = float(dist < u_distThreshold);
                                
                // yCbCr.yz = mix (vec2(0,0), yCbCr.yz, isSame);
                
                float t = u_frameCount;
                float timeWeight = sin(t / 2.0)*0.5 + 0.5; // between 0 and 1
                // timeWeight = mix (timeWeight*0.5, -timeWeight*0.8, float(hsv.z > 0.86));
                // float timeWeightedIntensity = hsv.z + timeWeight;
                float timeWeightedIntensity = timeWeight;
                hsv.z = mix (hsv.z, timeWeightedIntensity, isSame);
        
                vec4 transformedSRGB = sRGBAFromHSV(hsv, 1.0);
                Out_Color = transformedSRGB;
            }
        )";
        
        _highlightSameColorShader.initialize("Highlight Same Color", glslVersion, nullptr, fragmentShader_highlightSameColor);
        GLuint shaderHandle = _highlightSameColorShader.shaderHandle();
                
        _attribLocationRefColor = (GLuint)glGetUniformLocation(shaderHandle, "u_refColor");
        _attribLocationDistThreshold = (GLuint)glGetUniformLocation(shaderHandle, "u_distThreshold");
        _attribLocationFrameCount = (GLuint)glGetUniformLocation(shaderHandle, "u_frameCount");
        
        _highlightSameColorShader.setExtraUserCallback([this](GLShader& shader) {
            glUniform3f(_attribLocationRefColor, _activeColor.x, _activeColor.y, _activeColor.z);
            glUniform1f(_attribLocationDistThreshold, _deltaColorThreshold);
            glUniform1i(_attribLocationFrameCount, ImGui::GetFrameCount() / 2);
        });
    }
    
    void enableShader ()
    {
        if (_hasActiveColor)
            _highlightSameColorShader.enable();
    }
    
    void disableShader ()
    {
        if (_hasActiveColor)
            _highlightSameColorShader.disable();
    }
    
    void setSelectedPixel (float x, float y)
    {
        const auto newPixel = dl::vec2i((int)x, (int)y);
        
        if (_selectedPixel == newPixel)
        {
            // Toggle it.
            _hasActiveColor = false;
            _selectedPixel = dl::vec2i(-1,-1);
            return;
        }
        
        _selectedPixel = newPixel;
        dl::PixelSRGBA srgba = (*_im)(_selectedPixel.col, _selectedPixel.row);
        _activeColor.x = srgba.r / 255.f;
        _activeColor.y = srgba.g / 255.f;
        _activeColor.z = srgba.b / 255.f;
        _hasActiveColor = true;
    }
 
    void addSliderDelta (float delta)
    {
        dl_dbg ("delta = %f", delta);
        _deltaColorThreshold -= delta;
        _deltaColorThreshold = dl::keepInRange(_deltaColorThreshold, 1.f, 32.f);
    }
    
    void render ()
    {
        if (ImGui::Begin("Highlight Regions"))
        {
            ImGui::Text("Has active color = %d", _hasActiveColor);
            ImGui::Text("Active color = %d %d %d", (int)(255.f*_activeColor.x), (int)(255.f*_activeColor.y), (int)(255.f*_activeColor.z));
            float h,s,v;
            ImGui::ColorConvertRGBtoHSV(255.f*_activeColor.x, 255.f*_activeColor.y, 255.f*_activeColor.z, h, s, v);
            ImGui::Text("Active color HSV: [%.1fº %.1f%% %.1f]", h*360.f, s*100.f, v);
            int prevDeltaInt = int(_deltaColorThreshold + 0.5f);
            int deltaInt = prevDeltaInt;
            ImGui::SliderInt("Max Delta Color", &deltaInt, 1, 32);
            if (deltaInt != prevDeltaInt)
                _deltaColorThreshold = deltaInt;
        }
        ImGui::End();
    }
    
private:
    bool _hasActiveColor = false;
    ImVec4 _activeColor = ImVec4(0,0,0,1);
    float _deltaColorThreshold = 10.f;
    GLShader _highlightSameColorShader;
    
    GLuint _attribLocationRefColor = 0;
    GLuint _attribLocationDistThreshold = 0;
    GLuint _attribLocationFrameCount = 0;
    
    dl::ImageSRGBA* _im = nullptr;
    dl::vec2i _selectedPixel = dl::vec2i(0,0);
};

struct ImageViewerWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    
    DaltonViewerMode currentMode = DaltonViewerMode::None;
    
    HighlightRegion highlightRegion;
    
    bool shouldExit = false;
    GLFWwindow* window = nullptr;
    
    GLTexture gpuTexture;

    std::array<GLShader, 6> visualizationShaders;
    int currentVisualizationShaderIndex = 0;
    GLShader* currentVisualizationShader = &visualizationShaders[0];
    
    dl::ImageSRGBA im;
    std::string imagePath;
    
    ImVec2 monitorSize = ImVec2(-1,-1);
    
    struct {
        dl::Rect normal;
        dl::Rect current;
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

ImageViewerWindow::ImageViewerWindow()
: impl (new Impl())
{}

ImageViewerWindow::~ImageViewerWindow()
{
    dl_dbg("ImageViewerWindow::~ImageViewerWindow");
    
    shutdown();
}

bool ImageViewerWindow::isEnabled () const
{
    return impl->currentMode != DaltonViewerMode::None;
}

void ImageViewerWindow::shutdown()
{
    if (impl->window)
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(impl->imGuiContext);
        impl->imGuiContext = nullptr;
    }
}

bool ImageViewerWindow::initialize (int argc, char** argv, GLFWwindow* parentWindow)
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
            impl->windowSize.normal.size.x = w;
            impl->windowSize.normal.size.y = h;
            impl->windowSize.normal.origin.x = x;
            impl->windowSize.normal.origin.y = y;
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

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", impl->monitorSize.x, impl->monitorSize.y);

    if (impl->windowSize.normal.size.x < 0) impl->windowSize.normal.size.x = impl->im.width();
    if (impl->windowSize.normal.size.y < 0) impl->windowSize.normal.size.y = impl->im.height();
    if (impl->windowSize.normal.origin.x < 0) impl->windowSize.normal.origin.x = impl->monitorSize.x * 0.10;
    if (impl->windowSize.normal.origin.y < 0) impl->windowSize.normal.origin.y = impl->monitorSize.y * 0.10;
    impl->windowSize.current = impl->windowSize.normal;
    
    // Create window with graphics context.
    // We create a dummy window just because ImGui needs a main window, but we don't really
    // want any, because we prefer to rely on ImGui handling the viewport. This way we can
    // let the ImGui window resizeable, and the platform window will just get resized
    // accordingly. This way we can remove the decorations AND support resize.
    glfwWindowHint(GLFW_DECORATED, false);
    impl->window = glfwCreateWindow(1, 1, "Dear ImGui GLFW+OpenGL3 example", NULL, parentWindow);
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
    ImGui_ImplOpenGL3_Init(glslVersion());

    impl->gpuTexture.initialize();
    impl->gpuTexture.upload (impl->im);

    impl->visualizationShaders[0].initialize("Original", glslVersion(), nullptr, fragmentShader_Normal_glsl_130);
    impl->visualizationShaders[1].initialize("Daltonize - Protanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Protanope_glsl_130);
    impl->visualizationShaders[2].initialize("Daltonize - Deuteranope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Deuteranope_glsl_130);
    impl->visualizationShaders[3].initialize("Daltonize - Tritanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Tritanope_glsl_130);
    impl->visualizationShaders[4].initialize("Flip Red/Blue", glslVersion(), nullptr, fragmentShader_FlipRedBlue_glsl_130);
    impl->visualizationShaders[5].initialize("Flip Red/Blue and Invert Red", glslVersion(), nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130);
    
    impl->highlightRegion.initialize(glslVersion(), impl->im);
    
    return true;
}

void ImageViewerWindow::runOnce ()
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

    ImGui::SetCurrentContext(impl->imGuiContext);
    
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
            impl->currentVisualizationShader = &impl->visualizationShaders[impl->currentVisualizationShaderIndex];
        };

        if (ImGui::IsKeyPressed(GLFW_KEY_LEFT))
        {
            --impl->currentVisualizationShaderIndex;
            if (impl->currentVisualizationShaderIndex < 0)
            {
                impl->currentVisualizationShaderIndex = impl->visualizationShaders.size()-1;
            }
            updateCurrentShader();
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT))
        {
            ++impl->currentVisualizationShaderIndex;
            if (impl->currentVisualizationShaderIndex == impl->visualizationShaders.size())
            {
                impl->currentVisualizationShaderIndex = 0;
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
        float ratioX = impl->windowSize.current.size.x / impl->windowSize.normal.size.x;
        float ratioY = impl->windowSize.current.size.y / impl->windowSize.normal.size.y;
        if (ratioX < ratioY)
        {
            impl->windowSize.current.size.y = ratioX * impl->windowSize.normal.size.y;
        }
        else
        {
            impl->windowSize.current.size.x = ratioY * impl->windowSize.normal.size.x;
        }
        shouldUpdateWindowSize = true;
    }
    
    if (ImGui::IsKeyPressed(GLFW_KEY_D))
    {
        if (impl->currentMode == DaltonViewerMode::HighlightRegions)
        {
            impl->currentMode = DaltonViewerMode::Main;
        }
        else
        {
            impl->currentMode = DaltonViewerMode::HighlightRegions;
        }
    }
    
    if (io.InputQueueCharacters.contains('<'))
    {
        if (impl->windowSize.current.size.x > 64 && impl->windowSize.current.size.y > 64)
        {
            impl->windowSize.current.size.x *= 0.5f;
            impl->windowSize.current.size.y *= 0.5f;
            shouldUpdateWindowSize = true;
        }
    }
    
    if (io.InputQueueCharacters.contains('>'))
    {
        impl->windowSize.current.size.x *= 2.f;
        impl->windowSize.current.size.y *= 2.f;
        shouldUpdateWindowSize = true;
    }
    
    if (shouldUpdateWindowSize)
    {
        ImGui::SetNextWindowPos(ImVec2(impl->windowSize.current.origin.x, impl->windowSize.current.origin.y - titleBarHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(impl->windowSize.current.size.x, impl->windowSize.current.size.y + titleBarHeight), ImGuiCond_Always);
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
    
    std::string mainWindowName = "Invalid";
    switch (impl->currentMode)
    {
        case DaltonViewerMode::Main:
        {
            mainWindowName = impl->imagePath + " - " + impl->currentVisualizationShader->name();
            break;
        }
        case DaltonViewerMode::HighlightRegions:
        {
            mainWindowName = "Highlight Regions";
            break;
        }
    }
    
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
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
        impl->windowSize.current.size.x = ImGui::GetWindowWidth();
        impl->windowSize.current.size.y = ImGui::GetWindowHeight() - titleBarHeight;
        impl->windowSize.current.origin.x = ImGui::GetWindowPos().x;
        impl->windowSize.current.origin.y = ImGui::GetWindowPos().y + titleBarHeight;
        
        // ImGuiViewport* vp = ImGui::GetWindowViewport();
        // if (vp && ImGui::GetPlatformIO().Platform_SetWindowFocus)
        // {
        //     ImGui::GetPlatformIO().Platform_SetWindowFocus(vp);
        // }
        
        const auto contentSize = ImGui::GetContentRegionAvail();
        // dl_dbg ("contentSize: %f x %f", contentSize.x, contentSize.y);
        // dl_dbg ("imSize: %f x %f", imSize.x, imSize.y);
                
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
        
        switch (impl->currentMode)
        {
            case DaltonViewerMode::Main:
            {
                impl->currentVisualizationShader->enable();
                break;
            }
                
            case DaltonViewerMode::HighlightRegions:
            {
                impl->highlightRegion.enableShader();
                break;
            }
        }
        
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->gpuTexture.textureId()),
                     imSize(impl->windowSize.current),
                     uv0,
                     uv1);
        
        switch (impl->currentMode)
        {
            case DaltonViewerMode::Main:
            {
                impl->currentVisualizationShader->disable();
                break;
            }
                
            case DaltonViewerMode::HighlightRegions:
            {
                impl->highlightRegion.disableShader();
                break;
            }
        }
        
        ImVec2 mousePosInImage (0,0);
        ImVec2 mousePosInTexture (0,0);
        {
            // This 0.5 offset is important since the mouse coordinate is an integer.
            // So when we are in the center of a pixel we'll return 0,0 instead of
            // 0.5,0.5.
            ImVec2 widgetPos = (io.MousePos + ImVec2(0.5f,0.5f)) - widgetTopLeft;
            ImVec2 uv_window = widgetPos / imSize(impl->windowSize.current);
            mousePosInTexture = (uv1-uv0)*uv_window + uv0;
            mousePosInImage = mousePosInTexture * ImVec2(impl->im.width(), impl->im.height());
        }
        
        if (ImGui::IsItemHovered() && io.KeyAlt && impl->im.contains(mousePosInImage.x, mousePosInImage.y))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
            ImGui::BeginTooltip();
            const auto sRgb = impl->im((int)mousePosInImage.x, (int)mousePosInImage.y);
            const int zoomLenInPixels = 5; // most be odd
            const ImVec2 zoomLen_uv (float(zoomLenInPixels) / impl->im.width(), float(zoomLenInPixels) / impl->im.height());
            ImVec2 zoom_uv0 = mousePosInTexture - zoomLen_uv*0.5f;
            ImVec2 zoom_uv1 = mousePosInTexture + zoomLen_uv*0.5f;
            
            ImVec2 zoomItemSize (64,64);
            ImVec2 pixelSizeInZoom = zoomItemSize / ImVec2(zoomLenInPixels,zoomLenInPixels);
            
            ImVec2 zoomTopLeft = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(impl->gpuTexture.textureId()),
                         zoomItemSize,
                         zoom_uv0,
                         zoom_uv1);
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1 = pixelSizeInZoom * (zoomLenInPixels / 2) + zoomTopLeft;
            ImVec2 p2 = pixelSizeInZoom * ((zoomLenInPixels / 2) + 1) + zoomTopLeft;
            drawList->AddRect(p1, p2, IM_COL32(255,0,0,255));
            
            ImGui::Text("MousePosInImage: (%.2f, %.2f)", mousePosInImage.x, mousePosInImage.y);
            ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);
            const auto ycbcr = dl::convertToYCbCr(sRgb);
            ImGui::Text("yCbCr: [%d %d %d]", int(ycbcr.x), int(ycbcr.y), int(ycbcr.z));
            float h,s,v;
            ImGui::ColorConvertRGBtoHSV(sRgb.r, sRgb.g, sRgb.b, h, s, v);
            ImGui::Text("HSV: [%.1fº %.1f%% %.1f]", h*360.f, s*100.f, v);
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
                
        const bool noModifiers = !io.KeyCtrl && !io.KeyAlt && !io.KeySuper && !io.KeyShift;
        
        if (impl->currentMode == DaltonViewerMode::HighlightRegions)
        {
            // Accept Alt in case the user is still zooming in.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeySuper && !io.KeyShift)
            {
                impl->highlightRegion.setSelectedPixel(mousePosInImage.x, mousePosInImage.y);
            }
            
            if (io.MouseWheel != 0.f)
            {
                impl->highlightRegion.addSliderDelta (io.MouseWheel * 5.f);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    
    // ImGui::ShowDemoWindow();

    if (impl->currentMode == DaltonViewerMode::HighlightRegions)
        impl->highlightRegion.render();
    
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

bool ImageViewerWindow::shouldExit() const
{
    return impl->shouldExit;
}

} // dl
