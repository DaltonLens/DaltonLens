#include "ImageViewerWindow.h"
#include "GrabScreenAreaWindow.h"

#include "Graphics.h"
#include "ImGuiUtils.h"
#include "CrossPlatformUtils.h"

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
    None = -2,
    Original = -1,
    
    HighlightRegions = 0,
    Protanope,
    Deuteranope,
    Tritanope,
    FlipRedBlue,
    FlipRedBlueInvertRed,
    
    NumModes,
};

static std::string daltonViewerModeName (DaltonViewerMode mode)
{
    switch (mode)
    {
        case DaltonViewerMode::None: return "None";
        case DaltonViewerMode::Original: return "Original Image";
        case DaltonViewerMode::HighlightRegions: return "Highlight Same Color";
        case DaltonViewerMode::Protanope: return "Daltonize - Protanope";
        case DaltonViewerMode::Deuteranope: return "Daltonize - Deuteranope";
        case DaltonViewerMode::Tritanope: return "Daltonize - Tritanope";
        case DaltonViewerMode::FlipRedBlue: return "FlipRedBlue";
        case DaltonViewerMode::FlipRedBlueInvertRed: return "FlipRedBlueInvertRed";
        default: return "Invalid";
    }
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void helpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

struct HighlightRegion
{
    void initializeGL (const char* glslVersion)
    {
        const GLchar *fragmentShader_highlightSameColor = R"(
            uniform sampler2D Texture;
            uniform vec3 u_refColor;
            uniform float u_deltaH_360;
            uniform float u_deltaS_100;
            uniform float u_deltaV_255;
            uniform int u_frameCount;
            in vec2 Frag_UV;
            in vec4 Frag_Color;
            out vec4 Out_Color;
        
            bool checkHSVDelta(vec3 hsv1, vec3 hsv2)
            {
                vec3 diff = abs(hsv1 - hsv2);
                diff.x = min (diff.x, 1.0-diff.x); // h is modulo 360º
                return (diff.x*360.0    < u_deltaH_360
                        && diff.y*100.0 < u_deltaS_100
                        && diff.z*255.0 < u_deltaV_255);
            }
        
            void main()
            {
                vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
                vec3 hsv = HSVFromSRGB(srgba.rgb);                
                vec3 ref_hsv = HSVFromSRGB(u_refColor.rgb);
                
                // vec3 delta = abs(srgba.rgb - u_refColor.rgb);
                // float maxDelta = max(delta.r, max(delta.g, delta.b));
                bool isSame = checkHSVDelta(ref_hsv, hsv);
                                
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
        _attribLocationDeltaH = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaH_360");
        _attribLocationDeltaS = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaS_100");
        _attribLocationDeltaV = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaV_255");
        _attribLocationFrameCount = (GLuint)glGetUniformLocation(shaderHandle, "u_frameCount");
        
        _highlightSameColorShader.setExtraUserCallback([this](GLShader& shader) {
            glUniform3f(_attribLocationRefColor, _activeColorRGB01.x, _activeColorRGB01.y, _activeColorRGB01.z);
            glUniform1f(_attribLocationDeltaH, _deltaH_360);
            glUniform1f(_attribLocationDeltaS, _deltaS_100);
            glUniform1f(_attribLocationDeltaV, _deltaV_255);
            glUniform1i(_attribLocationFrameCount, ImGui::GetFrameCount() / 2);
        });
    }
    
    void setImage (dl::ImageSRGBA* im)
    {
        _im = im;
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
    
    bool hasActiveColor () const { return _hasActiveColor; }
    
    void clearSelection ()
    {
        _hasActiveColor = false;
        _selectedPixel = dl::vec2i(-1,-1);
        _activeColorRGB01 = ImVec4(0,0,0,1);
    }
    
    void setSelectedPixel (float x, float y)
    {
        const auto newPixel = dl::vec2i((int)x, (int)y);
        
        if (_hasActiveColor)
        {
            // Toggle it.
            clearSelection();
            return;
        }
        
        _selectedPixel = newPixel;
        dl::PixelSRGBA srgba = (*_im)(_selectedPixel.col, _selectedPixel.row);
        _activeColorRGB01.x = srgba.r / 255.f;
        _activeColorRGB01.y = srgba.g / 255.f;
        _activeColorRGB01.z = srgba.b / 255.f;
        
        _activeColorHSV_1_1_255 = dl::convertToHSV(srgba);
        
        _hasActiveColor = true;
        updateDeltas();
    }
 
    void addSliderDelta (float delta)
    {
        dl_dbg ("delta = %f", delta);
        _deltaColorThreshold -= delta;
        _deltaColorThreshold = dl::keepInRange(_deltaColorThreshold, 1.f, 20.f);
        updateDeltas();
    }
    
    void togglePlotMode ()
    {
        _plotMode = !_plotMode;
        updateDeltas();
    }
    
    void render ()
    {
        ImGuiWindowFlags flags = (/*ImGuiWindowFlags_NoTitleBar*/
                                // ImGuiWindowFlags_NoResize
                                // ImGuiWindowFlags_NoMove
                                // | ImGuiWindowFlags_NoScrollbar
                                ImGuiWindowFlags_NoScrollWithMouse
                                | ImGuiWindowFlags_AlwaysAutoResize
                                // | ImGuiWindowFlags_NoFocusOnAppearing
                                // | ImGuiWindowFlags_NoBringToFrontOnFocus
                                | ImGuiWindowFlags_NoNavFocus
                                | ImGuiWindowFlags_NoNavInputs
                                // | ImGuiWindowFlags_NoCollapse
                                // | ImGuiWindowFlags_NoBackground
                                // | ImGuiWindowFlags_NoSavedSettings
                                // | ImGuiWindowFlags_HorizontalScrollbar
                                | ImGuiWindowFlags_NoDocking
                                | ImGuiWindowFlags_NoNav);
        
        ImGui::SetNextWindowFocus(); // make sure it's always on top, otherwise it'll go behind the image.
        
        if (ImGui::Begin("DaltonLens - Selected color to Highlight", nullptr, flags))
        {
            const auto sRgb = dl::PixelSRGBA((int)(255.f*_activeColorRGB01.x + 0.5f), (int)(255.f*_activeColorRGB01.y + 0.5f), (int)(255.f*_activeColorRGB01.z + 0.5f), 255);
            
            const auto filledRectSize = ImVec2(128,128);
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + filledRectSize;
            
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32(sRgb.r, sRgb.g, sRgb.b, 255));
            drawList->AddRect(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32_WHITE);
            // Show a cross.
            if (!_hasActiveColor)
            {
                ImVec2 imageTopLeft = topLeft + screenFromWindow;
                ImVec2 imageBottomRight = bottomRight + screenFromWindow - ImVec2(1,1);
                drawList->AddLine(imageTopLeft, imageBottomRight, IM_COL32_WHITE);
                drawList->AddLine(ImVec2(imageTopLeft.x, imageBottomRight.y), ImVec2(imageBottomRight.x, imageTopLeft.y), IM_COL32_WHITE);
            }
                        
            ImGui::SetCursorPosX(bottomRight.x + 8);
            ImGui::SetCursorPosY(topLeft.y);
                        
            // Show the side info about the current color
            {
                ImGui::BeginChild("ColorInfo", ImVec2(196,filledRectSize.y));
                
                if (_hasActiveColor)
                {
                    auto closestColors = dl::closestColorEntries(sRgb, dl::ColorDistance::CIE2000);
                    
                    ImGui::Text("%s / %s",
                                closestColors[0].entry->className,
                                closestColors[0].entry->colorName);
                    
                    ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);
                    
                    PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
                    ImGui::Text("Linear RGB: [%d %d %d]", int(lrgb.r*255.0), int(lrgb.g*255.0), int(lrgb.b*255.0));
                    
                    auto hsv = _activeColorHSV_1_1_255;
                    ImGui::Text("HSV: [%.1fº %.1f%% %.1f]", hsv.x*360.f, hsv.y*100.f, hsv.z);
                    
                    PixelLab lab = dl::convertToLab(sRgb);
                    ImGui::Text("L*a*b: [%.1f %.1f %.1f]", lab.l, lab.a, lab.b);
                    
                    PixelXYZ xyz = convertToXYZ(sRgb);
                    ImGui::Text("XYZ: [%.1f %.1f %.1f]", xyz.x, xyz.y, xyz.z);
                }
                else
                {
                    ImGui::BulletText("Click on the image to\nhighlight pixels with\na similar color.");
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::BulletText("Right click on the image\nfor a contextual menu.");
                    ImGui::Dummy(ImVec2(0.0f, 5.0f));
                    ImGui::BulletText("Left/Right arrows to\nchange mode.");
                }
                
                ImGui::EndChild();
            }
            
            ImGui::SetCursorPosY(bottomRight.y + 8);
            
            ImGui::Text("Tip: try the mouse wheel to adjust the threshold.");
                        
            int prevDeltaInt = int(_deltaColorThreshold + 0.5f);
            int deltaInt = prevDeltaInt;
            ImGui::SliderInt("Max Difference", &deltaInt, 1, 20);
            if (deltaInt != prevDeltaInt)
            {
                _deltaColorThreshold = deltaInt;
                updateDeltas();
            }

            if (ImGui::Checkbox("Plot Mode", &_plotMode))
            {
                updateDeltas();
            }
            ImGui::SameLine();
            helpMarker("Allow more difference in saturation and value to better handle anti-aliasing on lines and curves. Better to disable it when looking at flat colors (e.g. pie charts).");
            
            if (_hasActiveColor)
            {
                ImGui::Text("Hue  in [%.0fº -> %.0fº]", (_activeColorHSV_1_1_255.x*360.f) - _deltaH_360, (_activeColorHSV_1_1_255.x*360.f) + _deltaH_360);
                ImGui::Text("Sat. in [%.0f%% -> %.0f%%]", (_activeColorHSV_1_1_255.y*100.f) - _deltaS_100, (_activeColorHSV_1_1_255.y*100.f) + _deltaS_100);
                ImGui::Text("Val. in [%.0f -> %.0f]", (_activeColorHSV_1_1_255.z) - _deltaV_255, (_activeColorHSV_1_1_255.z) + _deltaV_255);
            }
            else
            {
                ImGui::TextDisabled("Hue  in [N/A]");
                ImGui::TextDisabled("Sat. in [N/A]");
                ImGui::TextDisabled("Val. in [N/A]");
            }
        }
        ImGui::End();
    }
    
    void updateDeltas ()
    {
        // If the selected color is not saturated at all (grayscale), then we need rely on
        // value to discriminate the treat it the same way we treat the non-plot style.
        if (_plotMode && _activeColorHSV_1_1_255.y > 0.1)
        {
            _deltaH_360 = _deltaColorThreshold;
            
            // If the selected color is already not very saturated (like on aliased an edge), then don't tolerate a huge delta.
            _deltaS_100 = _deltaColorThreshold * 5.f; // tolerates 3x more since the range is [0,100]
            _deltaS_100 *= _activeColorHSV_1_1_255.y;
            _deltaS_100 = std::max(_deltaS_100, 1.0f);
            
            _deltaV_255 = _deltaColorThreshold*12.f; // tolerates slighly more since the range is [0,255]
        }
        else
        {
            _deltaH_360 = _deltaColorThreshold;
            _deltaS_100 = _deltaColorThreshold; // tolerates 3x more since the range is [0,100]
            _deltaV_255 = _deltaColorThreshold; // tolerates slighly more since the range is [0,255]
        }
    }
    
private:
    bool _hasActiveColor = false;
    ImVec4 _activeColorRGB01 = ImVec4(0,0,0,1);
    
    // H and S within [0,1]. V within [0,255]
    dl::PixelHSV _activeColorHSV_1_1_255 = dl::PixelHSV(0,0,0);
    
    float _deltaColorThreshold = 10.f;
    float _deltaH_360 = NAN; // within [0,360º]
    float _deltaS_100 = NAN; // within [0,100%]
    float _deltaV_255 = NAN; // within [0,255]
    
    bool _plotMode = true;
    GLShader _highlightSameColorShader;
    
    GLuint _attribLocationRefColor = 0;
    GLuint _attribLocationDeltaH = 0;
    GLuint _attribLocationDeltaS = 0;
    GLuint _attribLocationDeltaV = 0;
    GLuint _attribLocationFrameCount = 0;
    
    dl::ImageSRGBA* _im = nullptr;
    dl::vec2i _selectedPixel = dl::vec2i(0,0);
};

struct ImageViewerWindow::Impl
{
    ImGuiContext* imGuiContext = nullptr;
    ImGui_ImplGlfw_Context* imGuiContext_glfw = nullptr;
    ImGui_ImplOpenGL3_Context* imGuiContext_GL3 = nullptr;
    
    DaltonViewerMode currentMode = DaltonViewerMode::None;
    
    HighlightRegion highlightRegion;
    
    bool justUpdatedImage = false;
    
    bool helpWindowRequested = false;
    
    bool shouldExit = false;
    GLFWwindow* window = nullptr;
    
    struct {
        GLShader normal;
        GLShader protanope;
        GLShader deuteranope;
        GLShader tritanope;
        GLShader flipRedBlue;
        GLShader flipRedBlueAndInvertRed;
    } shaders;
    
    GLTexture gpuTexture;
    dl::ImageSRGBA im;
    std::string imagePath;
    
    ImVec2 monitorSize = ImVec2(-1,-1);
    
    const int windowBorderSize = 0;
    
    struct {
        dl::Rect normal;
        dl::Rect current;
    } imageWidgetRect;
    
    struct {
        int zoomFactor = 1;
        
        // UV means normalized between 0 and 1.
        ImVec2 uvCenter = ImVec2(0.5f,0.5f);
    } zoom;
    
    void enterMode (DaltonViewerMode newMode)
    {
        this->currentMode = newMode;
    }
    
    void cycleThroughMode (bool backwards)
    {
        int newMode_asInt = (int)this->currentMode;
        newMode_asInt += backwards ? -1 : 1;
        if (newMode_asInt < 0)
        {
            newMode_asInt = (int)DaltonViewerMode::NumModes-1;
        }
        else if (newMode_asInt == (int)DaltonViewerMode::NumModes)
        {
            newMode_asInt = 0;
        }
        
        // If we close that window, we might end up with a weird keyboard state
        // since a key release event might not be caught.
        if (this->currentMode == DaltonViewerMode::HighlightRegions)
        {
            resetKeyboardStateAfterWindowClose();
        }
        
        this->currentMode = (DaltonViewerMode)newMode_asInt;
    }
    
    void resetKeyboardStateAfterWindowClose ()
    {
        auto& io = ImGui::GetIO();
        
        // Reset the keyboard state to make sure we won't re-enter the next time
        // with 'q' or 'escape' already pressed from before.
        std::fill (io.KeysDown, io.KeysDown + sizeof(io.KeysDown)/sizeof(io.KeysDown[0]), false);
    }
    
    void onImageWidgetAreaChanged ()
    {
        glfwSetWindowSize(window, imageWidgetRect.current.size.x + windowBorderSize*2, imageWidgetRect.current.size.y + windowBorderSize*2);
    }
    
    void onWindowSizeChanged ()
    {
        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        imageWidgetRect.current.size.x = windowWidth - windowBorderSize*2;
        imageWidgetRect.current.size.y = windowHeight - windowBorderSize*2;
    }
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

bool ImageViewerWindow::helpWindowRequested () const
{
    return impl->helpWindowRequested;
}

void ImageViewerWindow::notifyHelpWindowRequestHandled ()
{
    impl->helpWindowRequested = false;
}

void ImageViewerWindow::dismiss()
{
    glfwSetWindowShouldClose(impl->window, true);
    impl->currentMode = DaltonViewerMode::None;
}

void ImageViewerWindow::shutdown()
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

bool ImageViewerWindow::initialize (GLFWwindow* parentWindow)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", impl->monitorSize.x, impl->monitorSize.y);

    // Create window with graphics context.
    glfwWindowHint(GLFW_DECORATED, true);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, true);
    glfwWindowHint(GLFW_FLOATING, false);
    glfwWindowHint(GLFW_RESIZABLE, false);
    impl->window = glfwCreateWindow(640, 480, "Dalton Lens Image Viewer", NULL, parentWindow);
    if (impl->window == NULL)
        return false;

    glfwSetWindowPos(impl->window, 0, 0);
    
    glfwMakeContextCurrent(impl->window);
    glfwHideWindow(impl->window);
    glfwSwapInterval(1); // Enable vsync

    dl::setWindowFlagsToAlwaysShowOnActiveDesktop(impl->window);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    impl->imGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(impl->imGuiContext);
    
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        // style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    impl->imGuiContext_glfw = ImGui_ImplGlfw_CreateContext();
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    
    impl->imGuiContext_GL3 = ImGui_ImplOpenGL3_CreateContext();
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());
    
    impl->shaders.normal.initialize("Original", glslVersion(), nullptr, fragmentShader_Normal_glsl_130);
    impl->shaders.protanope.initialize("Daltonize - Protanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Protanope_glsl_130);
    impl->shaders.deuteranope.initialize("Daltonize - Deuteranope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Deuteranope_glsl_130);
    impl->shaders.tritanope.initialize("Daltonize - Tritanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Tritanope_glsl_130);
    impl->shaders.flipRedBlue.initialize("Flip Red/Blue", glslVersion(), nullptr, fragmentShader_FlipRedBlue_glsl_130);
    impl->shaders.flipRedBlueAndInvertRed.initialize("Flip Red/Blue and Invert Red", glslVersion(), nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130);
    
    impl->highlightRegion.initializeGL(glslVersion());
    impl->gpuTexture.initialize();
    
    return true;
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
            impl->imageWidgetRect.normal.size.x = w;
            impl->imageWidgetRect.normal.size.y = h;
            impl->imageWidgetRect.normal.origin.x = x;
            impl->imageWidgetRect.normal.origin.y = y;
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
    
    initialize (parentWindow);

    if (impl->imageWidgetRect.normal.size.x < 0) impl->imageWidgetRect.normal.size.x = impl->im.width();
    if (impl->imageWidgetRect.normal.size.y < 0) impl->imageWidgetRect.normal.size.y = impl->im.height();
    if (impl->imageWidgetRect.normal.origin.x < 0) impl->imageWidgetRect.normal.origin.x = impl->monitorSize.x * 0.10;
    if (impl->imageWidgetRect.normal.origin.y < 0) impl->imageWidgetRect.normal.origin.y = impl->monitorSize.y * 0.10;
    impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
    
    impl->gpuTexture.upload (impl->im);
    impl->highlightRegion.setImage(&impl->im);
    
    impl->justUpdatedImage = true;
    
    return true;
}

void ImageViewerWindow::showGrabbedData (const GrabScreenData& grabbedData)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    
    impl->im.ensureAllocatedBufferForSize(grabbedData.srgbaImage->width(), grabbedData.srgbaImage->height());
    impl->im.copyDataFrom(*grabbedData.srgbaImage);
    impl->imagePath = "DaltonLens";

    // dl::writePngImage("/tmp/debug.png", impl->im);
    
    glfwMakeContextCurrent(impl->window);
    impl->gpuTexture.upload(impl->im);
    impl->imageWidgetRect.normal.origin = grabbedData.capturedScreenRect.origin;
    impl->imageWidgetRect.normal.size = grabbedData.capturedScreenRect.size;
    impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
    
    impl->highlightRegion.setImage(&impl->im);
    impl->currentMode = DaltonViewerMode::HighlightRegions;
    
    impl->justUpdatedImage = true;
}

void ImageViewerWindow::runOnce ()
{
    ImGui::SetCurrentContext(impl->imGuiContext);
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);

    glfwMakeContextCurrent(impl->window);
    
    if (impl->justUpdatedImage)
    {
        glfwSetWindowSize(impl->window, impl->imageWidgetRect.normal.size.x + 2*impl->windowBorderSize, impl->imageWidgetRect.normal.size.y + 2*impl->windowBorderSize);
        glfwSetWindowPos(impl->window, impl->imageWidgetRect.normal.origin.x - impl->windowBorderSize, impl->imageWidgetRect.normal.origin.y - impl->windowBorderSize);
    }
    
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

    bool popupMenuOpen = false;
    
    // First condition to update the window size is whether we just updated the content.
    bool shouldUpdateWindowSize = impl->justUpdatedImage;
    
    auto& io = ImGui::GetIO();
    
    auto modeForThisFrame = impl->currentMode;
    if (io.KeyShift)
    {
        modeForThisFrame = DaltonViewerMode::Original;
    }
    
    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE) || glfwWindowShouldClose(impl->window))
        {
            impl->currentMode = DaltonViewerMode::None;
            impl->highlightRegion.clearSelection();
            glfwSetWindowShouldClose(impl->window, false);
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_LEFT))
        {
            impl->cycleThroughMode (true /* backwards */);
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT) || ImGui::IsKeyPressed(GLFW_KEY_SPACE))
        {
            impl->cycleThroughMode (false /* not backwards */);
        }
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_N))
    {
        impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
        shouldUpdateWindowSize = true;
    }
    
    if (ImGui::IsKeyPressed(GLFW_KEY_A))
    {
        float ratioX = impl->imageWidgetRect.current.size.x / impl->imageWidgetRect.normal.size.x;
        float ratioY = impl->imageWidgetRect.current.size.y / impl->imageWidgetRect.normal.size.y;
        if (ratioX < ratioY)
        {
            impl->imageWidgetRect.current.size.y = ratioX * impl->imageWidgetRect.normal.size.y;
        }
        else
        {
            impl->imageWidgetRect.current.size.x = ratioY * impl->imageWidgetRect.normal.size.x;
        }
        shouldUpdateWindowSize = true;
    }
    
    if (io.InputQueueCharacters.contains('<'))
    {
        if (impl->imageWidgetRect.current.size.x > 64 && impl->imageWidgetRect.current.size.y > 64)
        {
            impl->imageWidgetRect.current.size.x *= 0.5f;
            impl->imageWidgetRect.current.size.y *= 0.5f;
            shouldUpdateWindowSize = true;
        }
    }
    
    if (io.InputQueueCharacters.contains('>'))
    {
        impl->imageWidgetRect.current.size.x *= 2.f;
        impl->imageWidgetRect.current.size.y *= 2.f;
        shouldUpdateWindowSize = true;
    }
                
    if (shouldUpdateWindowSize)
    {
        impl->onImageWidgetAreaChanged();
    }
    
    int platformWindowX, platformWindowY;
    glfwGetWindowPos(impl->window, &platformWindowX, &platformWindowY);
    
    int platformWindowWidth, platformWindowHeight;
    glfwGetWindowSize(impl->window, &platformWindowWidth, &platformWindowHeight);

    ImGui::SetNextWindowPos(ImVec2(platformWindowX, platformWindowY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(platformWindowWidth, platformWindowHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = (ImGuiWindowFlags_NoTitleBar
                            | ImGuiWindowFlags_NoResize
                            | ImGuiWindowFlags_NoMove
                            | ImGuiWindowFlags_NoScrollbar
                            // ImGuiWindowFlags_NoScrollWithMouse
                            // | ImGuiWindowFlags_NoCollapse
                            | ImGuiWindowFlags_NoBackground
                            | ImGuiWindowFlags_NoSavedSettings
                            | ImGuiWindowFlags_HorizontalScrollbar
                            // | ImGuiWindowFlags_NoDocking
                            | ImGuiWindowFlags_NoNav);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    // ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
    // ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    bool isOpen = true;
    
    std::string mainWindowName = impl->imagePath + " - " + daltonViewerModeName(modeForThisFrame);        
    glfwSetWindowTitle(impl->window, mainWindowName.c_str());
    
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
    {
        if (!isOpen)
        {
            impl->currentMode = DaltonViewerMode::None;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
        // Don't open the popup if Ctrl + right click was used, this is to zoom out.
        const bool ctrlKeyPressedAndMenuNotAlreadyOpen = (io.KeyCtrl && !popupMenuOpen);
        if (!ctrlKeyPressedAndMenuNotAlreadyOpen && ImGui::BeginPopupContextWindow())
        {
            popupMenuOpen = true;
            if (ImGui::MenuItem("Highlight Similar Colors")) impl->currentMode = DaltonViewerMode::HighlightRegions;
            if (ImGui::MenuItem("Daltonize - Protanope")) impl->currentMode = DaltonViewerMode::Protanope;
            if (ImGui::MenuItem("Daltonize - Deuteranope")) impl->currentMode = DaltonViewerMode::Deuteranope;
            if (ImGui::MenuItem("Daltonize - Tritanope")) impl->currentMode = DaltonViewerMode::Tritanope;
            if (ImGui::MenuItem("Flip Red & Blue")) impl->currentMode = DaltonViewerMode::FlipRedBlue;
            if (ImGui::MenuItem("Flip Red & Blue and Invert Red")) impl->currentMode = DaltonViewerMode::FlipRedBlueInvertRed;
            if (ImGui::MenuItem("Help")) impl->helpWindowRequested = true;
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
                        
        const auto contentSize = ImGui::GetContentRegionAvail();
//        dl_dbg ("contentSize: %f x %f", contentSize.x, contentSize.y);
//        dl_dbg ("imSize: %d x %d", impl->im.width(), impl->im.height());
//        dl_dbg ("windowSize.current: %f x %f", impl->imageArea.current.size.x, impl->imageArea.current.size.y);
//        dl_dbg ("framebufferScale: %f x %f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
//        dl_dbg ("fbDisplaySize: %d x %d", display_w, display_h);
                
        ImVec2 imageWidgetTopLeft = ImGui::GetCursorScreenPos();
        
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
        
        switch (modeForThisFrame)
        {
            case DaltonViewerMode::Original: impl->shaders.normal.enable(); break;
            case DaltonViewerMode::HighlightRegions: impl->highlightRegion.enableShader(); break;
            case DaltonViewerMode::Protanope: impl->shaders.protanope.enable(); break;
            case DaltonViewerMode::Deuteranope: impl->shaders.deuteranope.enable(); break;
            case DaltonViewerMode::Tritanope: impl->shaders.tritanope.enable(); break;
            case DaltonViewerMode::FlipRedBlue: impl->shaders.flipRedBlue.enable(); break;
            case DaltonViewerMode::FlipRedBlueInvertRed: impl->shaders.flipRedBlueAndInvertRed.enable(); break;
            default: break;
        }
        
        const auto imageWidgetSize = imSize(impl->imageWidgetRect.current);
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->gpuTexture.textureId()),
                     imageWidgetSize,
                     uv0,
                     uv1);
        
        switch (modeForThisFrame)
        {
            case DaltonViewerMode::Original: impl->shaders.normal.disable(); break;
            case DaltonViewerMode::HighlightRegions: impl->highlightRegion.disableShader(); break;
            case DaltonViewerMode::Protanope: impl->shaders.protanope.disable(); break;
            case DaltonViewerMode::Deuteranope: impl->shaders.deuteranope.disable(); break;
            case DaltonViewerMode::Tritanope: impl->shaders.tritanope.disable(); break;
            case DaltonViewerMode::FlipRedBlue: impl->shaders.flipRedBlue.disable(); break;
            case DaltonViewerMode::FlipRedBlueInvertRed: impl->shaders.flipRedBlueAndInvertRed.disable(); break;
            default: break;
        }
        
        ImVec2 mousePosInImage (0,0);
        ImVec2 mousePosInTexture (0,0);
        {
            // This 0.5 offset is important since the mouse coordinate is an integer.
            // So when we are in the center of a pixel we'll return 0,0 instead of
            // 0.5,0.5.
            ImVec2 widgetPos = (io.MousePos + ImVec2(0.5f,0.5f)) - imageWidgetTopLeft;
            ImVec2 uv_window = widgetPos / imageWidgetSize;
            mousePosInTexture = (uv1-uv0)*uv_window + uv0;
            mousePosInImage = mousePosInTexture * ImVec2(impl->im.width(), impl->im.height());
        }
        
        bool showCursorOverlay = !popupMenuOpen && ImGui::IsItemHovered() && impl->im.contains(mousePosInImage.x, mousePosInImage.y);
        if (showCursorOverlay)
        {
            bool showBecauseOfHighlightMode = (modeForThisFrame == DaltonViewerMode::HighlightRegions && !impl->highlightRegion.hasActiveColor());
            bool showBecauseOfShift = io.KeyShift;
            showCursorOverlay &= (showBecauseOfHighlightMode || showBecauseOfShift);
        }
        
        if (showCursorOverlay)
        {
            dl::showImageCursorOverlayTooptip (impl->im,
                                               impl->gpuTexture,
                                               imageWidgetTopLeft,
                                               imageWidgetSize,
                                               uv0,
                                               uv1,
                                               ImVec2(15,15));
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
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        {
            if (modeForThisFrame == DaltonViewerMode::HighlightRegions)
            {
                impl->highlightRegion.togglePlotMode();
            }
        }
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && io.KeyCtrl)
        {
            if (impl->zoom.zoomFactor >= 2)
                impl->zoom.zoomFactor /= 2;
        }
                
        const bool noModifiers = !io.KeyCtrl && !io.KeyAlt && !io.KeySuper && !io.KeyShift;
        
        if (modeForThisFrame == DaltonViewerMode::HighlightRegions)
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
//    ImGui::PopStyleVar();
//    ImGui::PopStyleVar();
//    ImGui::PopStyleVar();
    
    if (modeForThisFrame == DaltonViewerMode::HighlightRegions && !popupMenuOpen)
    {
        const int expectedHighlightWindowWidthWithPadding = 356;
        if (platformWindowX > expectedHighlightWindowWidthWithPadding)
        {
            // Put it on the left since there is room.
            ImGui::SetNextWindowPos(ImVec2(platformWindowX - expectedHighlightWindowWidthWithPadding, platformWindowY), ImGuiCond_Appearing);
        }
        else if ((impl->monitorSize.x - platformWindowX - platformWindowWidth) > expectedHighlightWindowWidthWithPadding)
        {
            // No room on the left, then put it on the right since there is room.
            ImGui::SetNextWindowPos(ImVec2(platformWindowX + platformWindowWidth + 8, platformWindowY), ImGuiCond_Appearing);
        }
        else
        {
            // All right, just leave the window inside the image one, which will be the default.
            // So do nothing.
        }
        impl->highlightRegion.render();
    }
    
    // ImGui::ShowDemoWindow();
    
    // Rendering
    ImGui::Render();

    // Not rendering any main window anymore.
    int display_w, display_h;
    glfwGetFramebufferSize(impl->window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.3, 0.3, 0.3, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Update and Render additional Platform Windows
    // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
    //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
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
    
    if (impl->justUpdatedImage)
    {
        // Only show it now, after the swap buffer. Otherwise we'll have the old image for a split second.
        glfwShowWindow(impl->window);
    }
    
    impl->justUpdatedImage = false;
    
    // User pressed q, escape or closed the window. We need to do an empty rendering to
    // make sure the platform windows will get hidden and won't stay as ghosts and create
    // flicker when we enable this again.
    if (impl->currentMode == DaltonViewerMode::None)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        // Reset the keyboard state to make sure we won't re-enter the next time
        // with 'q' or 'escape' already pressed from before.
        impl->resetKeyboardStateAfterWindowClose();

        ImGui::NewFrame();
        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        glfwHideWindow(impl->window);
    }
}

bool ImageViewerWindow::shouldExit() const
{
    return impl->shouldExit;
}

} // dl
