//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/MathUtils.h>

#include <memory>
#include <functional>

struct GLFWwindow;

struct ImGuiIO;

namespace dl
{

struct Rect;

/**
 * GLFW-backed window with its own ImGui and GL context.
 */
class ImguiGLFWWindow
{
public:
    struct FrameInfo
    {
        int windowContentWidth = -1;
        int windowContentHeight = -1;
        int frameBufferWidth = -1;
        int frameBufferHeight = -1;
        float contentDpiScale = 1.f;
    };

public:
    ImguiGLFWWindow();
    ~ImguiGLFWWindow();
    
public:
    static dl::Point primaryMonitorContentDpiScale ();
    static dl::Point primaryMonitorRetinaFrameBufferScale ();
    static void PushMonoSpaceFont(const ImGuiIO& io, bool small = false);
    static float monoFontSize (const ImGuiIO& io);

public:
    bool initialize (GLFWwindow* parentWindow,
                     const std::string& title,
                     const dl::Rect& geometry,
                     bool enableImguiViewports = false);
    void shutdown ();
    void runOnce ();

    void setEnabled (bool enabled);
    bool isEnabled () const;
    
    void setWindowPos (int x, int y);
    void setWindowSize (int width, int height);    
    
public:
    bool closeRequested () const;
    void cancelCloseRequest ();
    void triggerCloseRequest ();

    dl::Rect geometry() const;

public:
    FrameInfo beginFrame ();
    void endFrame ();

    void enableContexts ();
    void disableContexts ();

    GLFWwindow* glfwWindow ();

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
    friend class ImGuiScopedContext;
};

} // dl
