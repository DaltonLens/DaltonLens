//
//  DaltonLensGUI.cpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#include "DaltonLensGUI.h"

#include "ImageViewerWindow.h"
#include "PointerOverlayWindow.h"
#include "GrabScreenAreaWindow.h"
#include "CrossPlatformUtils.h"

#include <Dalton/Utils.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

namespace dl
{

struct DaltonLensGUI::Impl
{
    GLFWwindow* mainContextWindow = nullptr;
    
    ImageViewerWindow imageViewerWindow;
    PointerOverlayWindow pointerOverlayWindow;
    
    DisplayLinkTimer displayLinkTimer;
    KeyboardMonitor keyboardMonitor;
    
    void onDisplayLinkRefresh ()
    {
        if (pointerOverlayWindow.isEnabled())
            pointerOverlayWindow.runOnce();
        
        if (imageViewerWindow.isEnabled())
            imageViewerWindow.runOnce();
    }
};

DaltonLensGUI::DaltonLensGUI()
: impl (new Impl())
{
    
}

DaltonLensGUI::~DaltonLensGUI()
{
    impl->imageViewerWindow.shutdown();
    impl->pointerOverlayWindow.shutdown();
    
    if (impl->mainContextWindow)
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(impl->mainContextWindow);
        glfwTerminate();
    }
}

static void glfw_error_callback(int error, const char* description)
{
    dl_assert (false, "GLFW error %d: %s\n", error, description);
}

bool DaltonLensGUI::initialize ()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;
        
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    
    glfwWindowHint(GLFW_DECORATED, false);
    impl->mainContextWindow = glfwCreateWindow(1, 1, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
    if (impl->mainContextWindow == NULL)
        return false;
    
    glfwMakeContextCurrent(impl->mainContextWindow);
    
    // Initialize OpenGL loader
    // bool err = glewInit() != GLEW_OK;
    bool err = gl3wInit() != 0;
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return false;
    }
    
    glfwSwapInterval(1); // Enable vsync
    
    glfwSetWindowPos(impl->mainContextWindow, 0, 0);
    
    // Don't show that dummy window.
    glfwHideWindow (impl->mainContextWindow);
        
    impl->pointerOverlayWindow.initialize(impl->mainContextWindow);
    
    impl->displayLinkTimer.setCallback([this]() {
        impl->onDisplayLinkRefresh();
    });
    
    impl->keyboardMonitor.setKeyboardCtrlAltCmdFlagsCallback ([this](bool enabled) {
        impl->pointerOverlayWindow.setEnabled(enabled);
    });
    
    // Enabling that we lose the pointer overlay entirely.
//    char* argv[] = { "dummy" };
//    impl->imageViewerWindow.initialize(1, argv, impl->mainContextWindow);
    
    return true;
}

void DaltonLensGUI::shutdown ()
{
    
}

} // dl
