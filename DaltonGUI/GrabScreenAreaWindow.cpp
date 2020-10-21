#include "GrabScreenAreaWindow.h"

#include "Graphics.h"
#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>
#include <Dalton/ColorConversion.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA 1
#include <GLFW/glfw3native.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace  dl
{

struct RectSelection
{
    bool isValid () const { return topLeft.x >= 0.; }
    
    dl::Rect asDL () const
    {
        dl::Rect dlRect;
        dlRect.origin = dl::Point(topLeft.x, topLeft.y);
        dlRect.size = dl::Point(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
        return dlRect;
    }
    
    ImVec2 topLeft = ImVec2(-1,-1);
    ImVec2 bottomRight = ImVec2(-1,-1);
};

struct GrabScreenAreaWindow::Impl
{
    enum class State
    {
        Initial,
        MouseDragging
    };
    State currentState = State::Initial;
    
    ImGuiContext* imGuiContext = nullptr;
    ImGui_ImplGlfw_Context* imGuiContext_glfw = nullptr;
    ImGui_ImplOpenGL3_Context* imGuiContext_GL3 = nullptr;
    
    GLFWwindow* window = nullptr;

    bool justGotEnabled = false;
    
    bool grabbingFinished = true;
    GrabScreenData grabbedData;
    
    ImVec2 monitorWorkAreaSize = ImVec2(-1,-1);
    ImVec2 monitorWorkAreaTopLeft = ImVec2(-1,-1);
    
    ScreenGrabber grabber;
    
    RectSelection currentSelectionInScreen;
    
    void finishGrabbing ();
};

void GrabScreenAreaWindow::Impl::finishGrabbing ()
{
    this->grabbingFinished = true;
    this->grabbedData.isValid = this->currentSelectionInScreen.isValid();
    if (this->grabbedData.isValid)
    {
        this->grabbedData.capturedScreenRect = this->currentSelectionInScreen.asDL();
        dl::Rect capturedImageRect = this->grabbedData.capturedScreenRect;
        capturedImageRect.origin -= dl::Point(this->monitorWorkAreaTopLeft.x, this->monitorWorkAreaTopLeft.y);
        *this->grabbedData.srgbaImage = dl::crop (*this->grabbedData.srgbaImage, capturedImageRect);
    }
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    glfwHideWindow(this->window);
}

GrabScreenAreaWindow::GrabScreenAreaWindow()
: impl(new Impl())
{
    
}

GrabScreenAreaWindow::~GrabScreenAreaWindow()
{
    shutdown ();
}

bool GrabScreenAreaWindow::grabbingFinished() const
{
    return impl->grabbingFinished;
}

const GrabScreenData& GrabScreenAreaWindow::grabbedData () const
{
    return impl->grabbedData;
}

void GrabScreenAreaWindow::shutdown()
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
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, true);
    
    glfwWindowHint(GLFW_FLOATING, true);
        
//    window.collectionBehavior = [NSWindow.CollectionBehavior.stationary,
//                                 // NSWindowCollectionBehavior.canJoinAllSpaces,
//                                NSWindow.CollectionBehavior.moveToActiveSpace,
//                                NSWindow.CollectionBehavior.ignoresCycle];
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    
    {
        int xpos, ypos, width, height;
        glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &width, &height);
        impl->monitorWorkAreaSize = ImVec2(width, height);
        impl->monitorWorkAreaTopLeft = ImVec2(xpos, ypos);
    }
    
    impl->window = glfwCreateWindow(impl->monitorWorkAreaSize.x, impl->monitorWorkAreaSize.y, "DaltonLens Grab Screen", NULL, parentContext);
    if (impl->window == NULL)
        return false;
    
    glfwHideWindow(impl->window);
    
    glfwSetWindowPos(impl->window, impl->monitorWorkAreaTopLeft.x, impl->monitorWorkAreaTopLeft.y);
    
    NSWindow* nsWindow = (NSWindow*)glfwGetCocoaWindow(impl->window);
    dl_assert (nsWindow, "Not working?");
    nsWindow.collectionBehavior = nsWindow.collectionBehavior | NSWindowCollectionBehaviorMoveToActiveSpace | NSWindowCollectionBehaviorIgnoresCycle;
    
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
    
    impl->imGuiContext_glfw = ImGui_ImplGlfw_CreateContext();
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    
    impl->imGuiContext_GL3 = ImGui_ImplOpenGL3_CreateContext();
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
        
    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(impl->window, true);
    ImGui_ImplOpenGL3_Init(glslVersion());

    return true;
}

void showImageCursorOverlayTooptip (const dl::ImageSRGBA& image,
                                    GLTexture& imageTexture,
                                    ImVec2 imageWidgetTopLeft,
                                    ImVec2 imageWidgetSize,
                                    const ImVec2& uvTopLeft = ImVec2(0,0),
                                    const ImVec2& uvBottomRight = ImVec2(1,1),
                                    const ImVec2& roiWindowSize = ImVec2(15,15))
{
    auto& io = ImGui::GetIO();
    
    ImVec2 imageSize (image.width(), image.height());
    
    ImVec2 mousePosInImage (0,0);
    ImVec2 mousePosInTexture (0,0);
    {
        // This 0.5 offset is important since the mouse coordinate is an integer.
        // So when we are in the center of a pixel we'll return 0,0 instead of
        // 0.5,0.5.
        ImVec2 widgetPos = (io.MousePos + ImVec2(0.5f,0.5f)) - imageWidgetTopLeft;
        ImVec2 uv_window = widgetPos / imageWidgetSize;
        mousePosInTexture = (uvBottomRight-uvTopLeft)*uv_window + uvTopLeft;
        mousePosInImage = mousePosInTexture * imageSize;
    }
    
    if (!image.contains(mousePosInImage.x, mousePosInImage.y))
        return;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,8));
    {
        ImGui::BeginTooltip();
        
        const auto sRgb = image((int)mousePosInImage.x, (int)mousePosInImage.y);
        
        // Show the zoomed image.
        const ImVec2 zoomItemSize (128,128);
        {
            const int zoomLenInPixels = int(roiWindowSize.x);
            ImVec2 pixelSizeInZoom = zoomItemSize / ImVec2(zoomLenInPixels,zoomLenInPixels);
            
            const ImVec2 zoomLen_uv (float(zoomLenInPixels) / image.width(), float(zoomLenInPixels) / image.height());
            ImVec2 zoom_uv0 = mousePosInTexture - zoomLen_uv*0.5f;
            ImVec2 zoom_uv1 = mousePosInTexture + zoomLen_uv*0.5f;
            
            ImVec2 zoomImageTopLeft = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(imageTexture.textureId()), zoomItemSize, zoom_uv0, zoom_uv1);
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1 = pixelSizeInZoom * (zoomLenInPixels / 2) + zoomImageTopLeft;
            ImVec2 p2 = pixelSizeInZoom * ((zoomLenInPixels / 2) + 1) + zoomImageTopLeft;
            drawList->AddRect(p1, p2, IM_COL32(0,0,0,255));
            drawList->AddRect(p1 - ImVec2(1,1), p2 + ImVec2(1,1), IM_COL32(255,255,255,255));
        }
                
        ImGui::SameLine();
        
        // Show the central pixel color as a filled square
        {
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - ImGui::GetCursorPos();
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 bottomRight = topLeft + ImVec2(128,128);
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32(sRgb.r, sRgb.g, sRgb.b, 255));
            ImGui::SetCursorPosX(bottomRight.x + 8);
        }
        
        // Show the help
        {
            ImGui::BeginChild("ColorInfo", ImVec2(196,zoomItemSize.y));
            ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);

            PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
            ImGui::Text("Linear RGB: [%d %d %d]", int(lrgb.r*255.0), int(lrgb.g*255.0), int(lrgb.b*255.0));
            
            auto hsv = dl::convertToHSV(sRgb);
            ImGui::Text("HSV: [%.1fº %.1f%% %.1f]", hsv.x*360.f, hsv.y*100.f, hsv.z);
            
            PixelLab lab = dl::convertToLab(sRgb);
            ImGui::Text("L*a*b: [%.1f %.1f %.1f]", lab.l, lab.a, lab.b);
            
            PixelXYZ xyz = convertToXYZ(sRgb);
            ImGui::Text("XYZ: [%.1f %.1f %.1f]", xyz.x, xyz.y, xyz.z);
            
            ImGui::EndChild();
        }
        
        auto closestColors = dl::closestColorEntries(sRgb, dl::ColorDistance::CIE2000);
        
        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(16,16);
            
            // Raw draw is in screen space.
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[0].entry->r, closestColors[0].entry->g, closestColors[0].entry->b, 255));
            
            ImGui::SetCursorPosX(bottomRight.x + 8);
        }
        ImGui::Text("%s / %s [dist=%.1f] [%d %d %d]",
                    closestColors[0].entry->className,
                    closestColors[0].entry->colorName,
                    closestColors[0].distance,
                    closestColors[0].entry->r,
                    closestColors[0].entry->g,
                    closestColors[0].entry->b);
        
        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(16,16);
            
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[1].entry->r, closestColors[1].entry->g, closestColors[1].entry->b, 255));
            ImGui::SetCursorPosX(bottomRight.x + 8);
        }
        ImGui::Text("%s / %s [dist=%.1f] [%d %d %d]",
                    closestColors[1].entry->className,
                    closestColors[1].entry->colorName,
                    closestColors[1].distance,
                    closestColors[1].entry->r,
                    closestColors[1].entry->g,
                    closestColors[1].entry->b);
                
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
}

void GrabScreenAreaWindow::runOnce ()
{
    ImGui::SetCurrentContext(impl->imGuiContext);
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
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
    
    if (ImGui::IsKeyPressed(GLFW_KEY_Q) || ImGui::IsKeyPressed(GLFW_KEY_ESCAPE))
    {
        impl->finishGrabbing ();
        ImGui::Render();
        return;
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_SPACE))
    {
        dl::Rect frontWindowRect = dl::getFrontWindowGeometry();
        if (frontWindowRect.size.x >= 0)
        {
            impl->currentSelectionInScreen.topLeft = imPos(frontWindowRect);
            impl->currentSelectionInScreen.bottomRight = impl->currentSelectionInScreen.topLeft + imSize(frontWindowRect);
        }
    }
    
    if (ImGui::IsKeyReleased(GLFW_KEY_SPACE))
    {
        impl->finishGrabbing ();
        ImGui::Render();
        return;
    }
    
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        impl->currentState = Impl::State::MouseDragging;
        ImVec2 deltaFromInitial = ImGui::GetMouseDragDelta();
        impl->currentSelectionInScreen.topLeft = io.MousePos - deltaFromInitial + impl->monitorWorkAreaTopLeft;
        impl->currentSelectionInScreen.bottomRight = io.MousePos + impl->monitorWorkAreaTopLeft;
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (impl->currentState == Impl::State::MouseDragging)
        {
            impl->finishGrabbing ();
            ImGui::Render();
            return;
        }
    }
    
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
    
    dl_dbg ("Rendering the window.");
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(255,0,0,127));
    
    std::string mainWindowName = "GrabScreenArea";
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(impl->monitorWorkAreaSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    if (ImGui::Begin((mainWindowName + "###Image").c_str(), nullptr, flags))
    {
        {
            auto* drawList = ImGui::GetForegroundDrawList();
            drawList->AddCircleFilled(io.MousePos, 12, IM_COL32(0,0,0,64));
            drawList->AddLine(io.MousePos - ImVec2(16,0), io.MousePos + ImVec2(16,0), IM_COL32(0,0,0,255));
            drawList->AddLine(io.MousePos - ImVec2(0,16), io.MousePos + ImVec2(0,16), IM_COL32(0,0,0,255));
            drawList->AddRectFilled(io.MousePos, io.MousePos + ImVec2(1,1), IM_COL32_WHITE);
        }
        
        const auto imageWidgetTopLeft = ImGui::GetCursorPos();
        const auto imageWidgetSize = impl->monitorWorkAreaSize;
        ImGui::Image(reinterpret_cast<ImTextureID>(impl->grabbedData.texture->textureId()), imageWidgetSize);
        
        showImageCursorOverlayTooptip (*impl->grabbedData.srgbaImage,
                                       *impl->grabbedData.texture,
                                       imageWidgetTopLeft,
                                       imageWidgetSize);
        
        ImVec2 selectionTopLeftInWindow = impl->currentSelectionInScreen.topLeft - impl->monitorWorkAreaTopLeft;
        ImVec2 selectionBottomRightInWindow = impl->currentSelectionInScreen.bottomRight - impl->monitorWorkAreaTopLeft;
            
        if (impl->currentSelectionInScreen.isValid())
        {
            auto* drawList = ImGui::GetWindowDrawList();
            
            // Border around the selected area.
            drawList->AddRectFilled(selectionTopLeftInWindow, selectionBottomRightInWindow, IM_COL32(0, 0, 127, 64));
            
            //            // Left rectangle
            //            drawList->AddRectFilled(ImVec2(0,0), ImVec2(selectionTopLeftInWindow.x, impl->monitorWorkAreaSize.y), IM_COL32(0, 0, 0, 127));
            //
            //            // Right rectangle
            //            drawList->AddRectFilled(ImVec2(selectionBottomRightInWindow.x,0), ImVec2(impl->monitorWorkAreaSize.x, impl->monitorWorkAreaSize.y), IM_COL32(0, 0, 0, 127));
            //
            //            // Top rectangle
            //            drawList->AddRectFilled(ImVec2(selectionTopLeftInWindow.x,0), ImVec2(selectionBottomRightInWindow.x, selectionTopLeftInWindow.y), IM_COL32(0, 0, 0, 127));
            //
            //            // Bottom rectangle
            //            drawList->AddRectFilled(ImVec2(selectionTopLeftInWindow.x,selectionBottomRightInWindow.y), ImVec2(selectionBottomRightInWindow.x, impl->monitorWorkAreaSize.y), IM_COL32(0, 0, 0, 127));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    
    ImGui::PopStyleColor();
        
    // Rendering
    ImGui::Render();
    
    // Not rendering any main window anymore.
    int display_w, display_h;
    glfwGetFramebufferSize(impl->window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(impl->window);
    
    if (impl->justGotEnabled)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        glfwShowWindow(impl->window);
        impl->justGotEnabled = false;
    }
}

bool GrabScreenAreaWindow::startGrabbing ()
{
    ImGui::SetCurrentContext(impl->imGuiContext);
    ImGui_ImplGlfw_SetCurrentContext(impl->imGuiContext_glfw);
    ImGui_ImplOpenGL3_SetCurrentContext(impl->imGuiContext_GL3);
    
    glfwMakeContextCurrent(impl->window);
    
    impl->justGotEnabled = true;
    impl->grabbingFinished = false;
    
    impl->currentSelectionInScreen = {};
    
    dl::Rect screenRect;
    screenRect.origin = dl::Point(impl->monitorWorkAreaTopLeft.x, impl->monitorWorkAreaTopLeft.y);
    screenRect.size = dl::Point(impl->monitorWorkAreaSize.x, impl->monitorWorkAreaSize.y);
    
    impl->grabbedData = {};
    impl->grabbedData.srgbaImage = std::make_shared<dl::ImageSRGBA>();
    impl->grabbedData.texture = std::make_shared<GLTexture>();
    
    return (impl->grabber.grabScreenArea (screenRect, *impl->grabbedData.srgbaImage, *impl->grabbedData.texture));
}

} // dl
