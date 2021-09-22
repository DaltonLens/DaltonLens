//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#define GL_SILENCE_DEPRECATION 1

#include "ImageViewerWindow.h"
#include "ImageViewerWindowState.h"

#include <DaltonGUI/GrabScreenAreaWindow.h>
#include <DaltonGUI/Graphics.h>
#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/CrossPlatformUtils.h>
#include <DaltonGUI/ImguiGLFWWindow.h>
#include <DaltonGUI/HighlightSimilarColor.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/ColorConversion.h>

// Note: need to include that before GLFW3 for some reason.
#include <GL/gl3w.h>
#include <DaltonGUI/GLFWUtils.h>

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

std::string daltonViewerModeName (DaltonViewerMode mode)
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

struct ImageViewerWindow::Impl
{
    ImguiGLFWWindow imguiGlfwWindow;
    ImageViewerObserver* observer = nullptr;
    
    ImageViewerWindowState mutableState;

    bool enabled = false;

    HighlightRegionShader highlightRegionShader;

    ImageCursorOverlay cursorOverlay;

    struct {
        bool inProgress = false;
        bool needToResize = false;
        int numAlreadyRenderedFrames = 0;
        dl::Rect targetWindowGeometry;

        void setCompleted () { *this = {}; }
    } updateAfterContentSwitch;
    
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
    bool shouldUpdateWindowSize = false;
    
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
        this->mutableState.currentMode = newMode;
    }
    
    void advanceMode (bool backwards)
    {
        int newMode_asInt = (int)this->mutableState.currentMode;
        newMode_asInt += backwards ? -1 : 1;
        if (newMode_asInt < 0)
        {
            newMode_asInt = 0;
        }
        else if (newMode_asInt == (int)DaltonViewerMode::NumModes)
        {
            --newMode_asInt;
        }
        
        // If we close that window, we might end up with a weird keyboard state
        // since a key release event might not be caught.
        if (mutableState.currentMode == DaltonViewerMode::HighlightRegions)
        {
            resetKeyboardStateAfterWindowClose();
        }
        
        mutableState.currentMode = (DaltonViewerMode)newMode_asInt;
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
        imguiGlfwWindow.setWindowSize(imageWidgetRect.current.size.x + windowBorderSize * 2,
                                      imageWidgetRect.current.size.y + windowBorderSize * 2);
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
    return impl->enabled;
}

void ImageViewerWindow::setEnabled (bool enabled)
{
    if (impl->enabled == enabled)
        return;

    impl->enabled = enabled;

    if (enabled)
    {
        impl->imguiGlfwWindow.setEnabled(true);
    }
    else
    {
        impl->mutableState.currentMode = DaltonViewerMode::None;
        impl->imguiGlfwWindow.setEnabled(false);
    }
}

void ImageViewerWindow::shutdown()
{
    impl->imguiGlfwWindow.shutdown ();
}

bool ImageViewerWindow::initialize (GLFWwindow* parentWindow, ImageViewerObserver* observer)
{
    impl->observer = observer;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    dl_dbg ("Primary monitor size = %f x %f", impl->monitorSize.x, impl->monitorSize.y);

    // Create window with graphics context.
    // glfwWindowHint(GLFW_RESIZABLE, false); // fixed size.

    dl::Rect windowGeometry;
    windowGeometry.origin.x = 0;
    windowGeometry.origin.y = 0;
    windowGeometry.size.x = 640;
    windowGeometry.size.y = 480;
    
    if (!impl->imguiGlfwWindow.initialize (parentWindow, "Dalton Lens Image Viewer", windowGeometry, false /* viewports */))
        return false;

    glfwWindowHint(GLFW_RESIZABLE, true); // restore the default.

    dl::setWindowFlagsToAlwaysShowOnActiveDesktop(impl->imguiGlfwWindow.glfwWindow());        
    
    impl->shaders.normal.initialize("Original", glslVersion(), nullptr, fragmentShader_Normal_glsl_130);
    impl->shaders.protanope.initialize("Daltonize - Protanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Protanope_glsl_130);
    impl->shaders.deuteranope.initialize("Daltonize - Deuteranope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Deuteranope_glsl_130);
    impl->shaders.tritanope.initialize("Daltonize - Tritanope", glslVersion(), nullptr, fragmentShader_DaltonizeV1_Tritanope_glsl_130);
    impl->shaders.flipRedBlue.initialize("Flip Red/Blue", glslVersion(), nullptr, fragmentShader_FlipRedBlue_glsl_130);
    impl->shaders.flipRedBlueAndInvertRed.initialize("Flip Red/Blue and Invert Red", glslVersion(), nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130);
    
    impl->highlightRegionShader.initializeGL (glslVersion());
    impl->gpuTexture.initialize();
    
    return true;
}

ImageViewerWindowState& ImageViewerWindow::mutableState ()
{
    return impl->mutableState;
}

void ImageViewerWindow::checkImguiGlobalImageKeyEvents ()
{
    // These key events are valid also in the control window.
    auto& io = ImGui::GetIO();

    for (const auto code : {GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_N, GLFW_KEY_A})
    {
        if (ImGui::IsKeyPressed(code))
            processKeyEvent(code);
    }

    // Those don't have direct GLFW keycodes for some reason.
    for (const auto c : {'<', '>'})
    {
        if (io.InputQueueCharacters.contains(c))
            processKeyEvent(c);
    }
}

void ImageViewerWindow::processKeyEvent (int keycode)
{
    switch (keycode)
    {
        case GLFW_KEY_UP: 
        {
            impl->advanceMode(true /* backwards */); 
            break;
        }

        case GLFW_KEY_DOWN:
        {
            impl->advanceMode(false /* forward */);
            break;
        }

        case GLFW_KEY_N: 
        {
            impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
            impl->shouldUpdateWindowSize = true;
            break;
        }

        case GLFW_KEY_A:
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
            impl->shouldUpdateWindowSize = true;
            break;
        }

        case '<':
        {
            if (impl->imageWidgetRect.current.size.x > 64 && impl->imageWidgetRect.current.size.y > 64)
            {
                impl->imageWidgetRect.current.size.x *= 0.5f;
                impl->imageWidgetRect.current.size.y *= 0.5f;
                impl->shouldUpdateWindowSize = true;
            }
            break;
        }

        case '>':
        {
            impl->imageWidgetRect.current.size.x *= 2.f;
            impl->imageWidgetRect.current.size.y *= 2.f;
            impl->shouldUpdateWindowSize = true;
            break;
        }
    }
}

dl::Rect ImageViewerWindow::geometry () const
{
    return impl->imguiGlfwWindow.geometry();
}

void ImageViewerWindow::showGrabbedData (const GrabScreenData& grabbedData, dl::Rect& updatedWindowGeometry)
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    impl->monitorSize = ImVec2(mode->width, mode->height);
    
    impl->im.ensureAllocatedBufferForSize(grabbedData.srgbaImage->width(), grabbedData.srgbaImage->height());
    impl->im.copyDataFrom(*grabbedData.srgbaImage);
    impl->imagePath = "DaltonLens";

    // dl::writePngImage("/tmp/debug.png", impl->im);
    
    impl->imguiGlfwWindow.enableContexts ();
    impl->gpuTexture.upload(impl->im);
    impl->imageWidgetRect.normal.origin = grabbedData.capturedScreenRect.origin;
    impl->imageWidgetRect.normal.size = grabbedData.capturedScreenRect.size;
    impl->imageWidgetRect.current = impl->imageWidgetRect.normal;
    
    impl->mutableState.highlightRegion.setImage(&impl->im);
    impl->mutableState.currentMode = DaltonViewerMode::HighlightRegions;
    
    // Don't show it now, but tell it to show the window after
    // updating the content, otherwise we can get annoying flicker.
    impl->updateAfterContentSwitch.inProgress = true;
    impl->updateAfterContentSwitch.needToResize = true;
    impl->updateAfterContentSwitch.numAlreadyRenderedFrames = 0;
    impl->updateAfterContentSwitch.targetWindowGeometry.origin.x = impl->imageWidgetRect.normal.origin.x - impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.origin.y = impl->imageWidgetRect.normal.origin.y - impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.size.x = impl->imageWidgetRect.normal.size.x + 2 * impl->windowBorderSize;
    impl->updateAfterContentSwitch.targetWindowGeometry.size.y = impl->imageWidgetRect.normal.size.y + 2 * impl->windowBorderSize;

    updatedWindowGeometry = impl->updateAfterContentSwitch.targetWindowGeometry;

}

void ImageViewerWindow::runOnce ()
{
    if (impl->updateAfterContentSwitch.needToResize)
    {
        impl->imguiGlfwWindow.enableContexts();
        dl_dbg ("ImageWindow set to %d x %d (%d + %d)", 
            int(impl->imageWidgetRect.normal.size.x), 
            int(impl->imageWidgetRect.normal.size.y),
            int(impl->imageWidgetRect.normal.origin.x - impl->windowBorderSize), 
            int(impl->imageWidgetRect.normal.origin.y - impl->windowBorderSize));

        impl->imguiGlfwWindow.setWindowSize (impl->updateAfterContentSwitch.targetWindowGeometry.size.x, 
                                             impl->updateAfterContentSwitch.targetWindowGeometry.size.y);
        
        dl_dbg("[VIEWERWINDOW] new size = %d x %d",
               int(impl->imageWidgetRect.normal.size.x + 2 * impl->windowBorderSize), 
               int(impl->imageWidgetRect.normal.size.y + 2 * impl->windowBorderSize));

        impl->updateAfterContentSwitch.needToResize = false;
    }

    const auto frameInfo = impl->imguiGlfwWindow.beginFrame ();    

    // If we do not have a pending resize request, then adjust the content size to the
    // actual window size. The framebuffer might be bigger depending on the retina scale
    // factor.
    if (!impl->shouldUpdateWindowSize)
    {
        const float dpiScale = ImGui::GetWindowDpiScale();
        impl->imageWidgetRect.current.size.x = frameInfo.frameBufferWidth / dpiScale;
        impl->imageWidgetRect.current.size.y = frameInfo.frameBufferHeight / dpiScale;
    }

    impl->mutableState.highlightRegion.updateFrameCount ();

    bool popupMenuOpen = false;
    
    auto& io = ImGui::GetIO();
    
    auto modeForThisFrame = impl->mutableState.currentMode;
    
    if (io.KeyShift)
    {
        modeForThisFrame = DaltonViewerMode::Original;
    }
    
    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE) || impl->imguiGlfwWindow.closeRequested())
        {
            impl->mutableState.currentMode = DaltonViewerMode::None;
            impl->mutableState.highlightRegion.clearSelection();
            impl->imguiGlfwWindow.cancelCloseRequest ();
        }

        checkImguiGlobalImageKeyEvents ();
    }
    
    if (impl->shouldUpdateWindowSize)
    {
        impl->onImageWidgetAreaChanged();
        impl->shouldUpdateWindowSize = false;
    }
    
    dl::Rect platformWindowGeometry = impl->imguiGlfwWindow.geometry();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(frameInfo.frameBufferWidth, frameInfo.frameBufferHeight), ImGuiCond_Always);

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
    bool isOpen = true;
    
    std::string mainWindowName = impl->imagePath + " - " + daltonViewerModeName(modeForThisFrame);        
    glfwSetWindowTitle(impl->imguiGlfwWindow.glfwWindow(), mainWindowName.c_str());

    if (ImGui::Begin((mainWindowName + "###Image").c_str(), &isOpen, flags))
    {
        if (!isOpen)
        {
            impl->mutableState.currentMode = DaltonViewerMode::None;
        }
                      
        const ImVec2 imageWidgetTopLeft = ImGui::GetCursorScreenPos();
        
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
            case DaltonViewerMode::HighlightRegions: impl->highlightRegionShader.enableShader(impl->mutableState.highlightRegion.mutableData.shaderParams); break;
            case DaltonViewerMode::Protanope: impl->shaders.protanope.enable(); break;
            case DaltonViewerMode::Deuteranope: impl->shaders.deuteranope.enable(); break;
            case DaltonViewerMode::Tritanope: impl->shaders.tritanope.enable(); break;
            case DaltonViewerMode::FlipRedBlue: impl->shaders.flipRedBlue.enable(); break;
            case DaltonViewerMode::FlipRedBlueInvertRed: impl->shaders.flipRedBlueAndInvertRed.enable(); break;
            default: break;
        }
        
        const bool imageHasNonMultipleSize = int(impl->imageWidgetRect.current.size.x) % int(impl->imageWidgetRect.normal.size.x) != 0;
        const bool hasZoom = impl->zoom.zoomFactor != 1;
        const bool useLinearFiltering = imageHasNonMultipleSize && !hasZoom;
        // Enable it just for that rendering otherwise the pointer overlay will get filtered too.
        if (useLinearFiltering)
        {
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                                                    {
                                                        ImageViewerWindow *that = reinterpret_cast<ImageViewerWindow *>(cmd->UserCallbackData);
                                                        that->impl->gpuTexture.setLinearInterpolationEnabled(true);
                                                    },
                                                    this);
        }

        const auto imageWidgetSize = imSize(impl->imageWidgetRect.current);
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->gpuTexture.textureId()),
                     imageWidgetSize,
                     uv0,
                     uv1);        

        if (useLinearFiltering)
        {
            ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                                                    {
                                                        ImageViewerWindow *that = reinterpret_cast<ImageViewerWindow *>(cmd->UserCallbackData);
                                                        that->impl->gpuTexture.setLinearInterpolationEnabled(false);
                                                    },
                                                    this);
        }

        switch (modeForThisFrame)
        {
            case DaltonViewerMode::Original: impl->shaders.normal.disable(); break;
            case DaltonViewerMode::HighlightRegions: impl->highlightRegionShader.disableShader(); break;
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
            bool showBecauseOfHighlightMode = (modeForThisFrame == DaltonViewerMode::HighlightRegions && !impl->mutableState.highlightRegion.hasActiveColor());
            bool showBecauseOfShift = io.KeyShift;
            showCursorOverlay &= (showBecauseOfHighlightMode || showBecauseOfShift);
        }
        
        if (showCursorOverlay)
        {
            impl->cursorOverlay.showTooltip(impl->im,
                                            impl->gpuTexture,
                                            imageWidgetTopLeft,
                                            imageWidgetSize,
                                            uv0,
                                            uv1,
                                            ImVec2(15, 15));
        }    

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
                impl->mutableState.highlightRegion.togglePlotMode();
            }
        }
        
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            if (io.KeyCtrl)
            {
                if (impl->zoom.zoomFactor >= 2)
                    impl->zoom.zoomFactor /= 2;
            }
            else
            {
                // xv-like controls focus.
                if (impl->observer) impl->observer->onControlsRequested();
            }
        }

        if (modeForThisFrame == DaltonViewerMode::HighlightRegions)
        {
            // Accept Alt in case the user is still zooming in.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeySuper && !io.KeyShift)
            {
                impl->mutableState.highlightRegion.setSelectedPixel(mousePosInImage.x, mousePosInImage.y);
            }
            
            if (io.MouseWheel != 0.f)
            {
                impl->mutableState.highlightRegion.addSliderDelta (io.MouseWheel * 5.f);
            }
        }
    }
        
    ImGui::End();
    ImGui::PopStyleVar();
    
    if (impl->mutableState.currentMode == DaltonViewerMode::HighlightRegions && !popupMenuOpen)
    {
        const int expectedHighlightWindowWidthWithPadding = 364;
        if (platformWindowGeometry.origin.x > expectedHighlightWindowWidthWithPadding)
        {
            // Put it on the left since there is room.
            ImGui::SetNextWindowPos(ImVec2(platformWindowGeometry.origin.x - expectedHighlightWindowWidthWithPadding,
                                           platformWindowGeometry.origin.y), 
                                    ImGuiCond_Appearing);
        }
        else if ((impl->monitorSize.x - platformWindowGeometry.origin.x - platformWindowGeometry.size.x) > expectedHighlightWindowWidthWithPadding)
        {
            // No room on the left, then put it on the right since there is room.
            ImGui::SetNextWindowPos(ImVec2(platformWindowGeometry.origin.x + platformWindowGeometry.size.x + 8,
                                           platformWindowGeometry.origin.y),
                                    ImGuiCond_Appearing);
        }
        else
        {
            // All right, just leave the window inside the image one, which will be the default.
            // So do nothing.
        }
        
        // Note: better to collapse it than disable it because the handling of the shift key
        // was buggy, the key release event would not get caught if the press happened on the
        // highlight window.
        const bool collapsed = (modeForThisFrame != DaltonViewerMode::HighlightRegions);
        renderHighlightRegionControls (impl->mutableState.highlightRegion, collapsed);
    }
    
    impl->imguiGlfwWindow.endFrame ();

    if (impl->updateAfterContentSwitch.inProgress)
    {
        ++impl->updateAfterContentSwitch.numAlreadyRenderedFrames;
        
        if (impl->updateAfterContentSwitch.numAlreadyRenderedFrames >= 2)
        {
            setEnabled(true);
            impl->imguiGlfwWindow.setWindowPos(impl->updateAfterContentSwitch.targetWindowGeometry.origin.x,
                                               impl->updateAfterContentSwitch.targetWindowGeometry.origin.y);
            impl->updateAfterContentSwitch.setCompleted(); // not really needed, just to be explicit.
        }
    }
    
    // User pressed q, escape or closed the window. We need to do an empty rendering to
    // make sure the platform windows will get hidden and won't stay as ghosts and create
    // flicker when we enable this again.
    if (impl->mutableState.currentMode == DaltonViewerMode::None)
    {
        if (impl->observer) impl->observer->onDismissRequested ();
    }
}

} // dl
