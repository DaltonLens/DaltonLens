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

struct OverlayTriggerEventDetector
{
    enum State {
        Initial = 0,
        GotFirstPress = 1,
        GotFirstRelease = 2,
        GotSecondPress = 3,
        GotSecondRelease = 4
    };
    
    void onCtrlAltCmdFlagsChanged(bool enabled)
    {
        ctrlAltCmdWasLastEnabled = enabled;
        
        // 500ms to consider a keypress and not a keyhold.
        const double maxKeyPressTime = 0.5;
        const double maxTimeBetweenPresses = 0.5;
        
        const double nowSeconds = dl::currentDateInSeconds();
        
        dl_dbg ("BEFORE _currentState = %d", _currentState);
        
        // Make sure to go back to initial if we haven't seen any event for too long.
        if (_currentState != GotSecondRelease && nowSeconds - timeWhenLastStateChanged > maxTimeBetweenPresses)
        {
            _currentState = Initial;
        }
        
        switch (_currentState)
        {
            case Initial:
            {
                if (enabled)
                {
                    _currentState = GotFirstPress;
                    timeWhenLastStateChanged = nowSeconds;
                }
                break;
            }
                
            case GotFirstPress:
            {
                if (enabled) { _currentState = Initial; break; }

                if ((nowSeconds - timeWhenLastStateChanged) > maxKeyPressTime)
                {
                    _currentState = Initial;
                }
                else
                {
                    timeWhenLastStateChanged = nowSeconds;
                    _currentState = GotFirstRelease;
                }
                break;
            }
                
            case GotFirstRelease:
            {
                // We get multiple release events since we don't release the keys all at once.
                if (!enabled) { break; }
                
                dl_dbg("nowSeconds = %f", nowSeconds);
                dl_dbg("timeWhenLastStateChanged = %f", timeWhenLastStateChanged);
                
                if ((nowSeconds - timeWhenLastStateChanged) > maxTimeBetweenPresses)
                {
                    _currentState = Initial; break;
                }
                
                _currentState = GotSecondPress;
                timeWhenLastStateChanged = nowSeconds;
                break;
            }
                
            case GotSecondPress:
            {
                if (enabled) { _currentState = Initial; break; }

                if ((nowSeconds - timeWhenLastStateChanged) > maxKeyPressTime)
                {
                    _currentState = Initial;
                }
                else
                {
                    _currentState = GotSecondRelease;
                    timeWhenLastStateChanged = nowSeconds;
                }
                break;
            }
                
            case GotSecondRelease:
            {
                if (enabled)
                {
                    _currentState = Initial;
                }
                break;
            }
        }
        
        dl_dbg ("AFTER _currentState = %d", _currentState);
    }
    
    bool isEnabled () const
    {
        return _currentState == GotSecondRelease;
    }
    
private:
    State _currentState = State::Initial;
    bool ctrlAltCmdWasLastEnabled = false;
    double timeWhenLastStateChanged = NAN;
};

struct DaltonLensGUI::Impl
{
    enum class State {
        Disabled,
        GrabScreen,
        ImageViewer,
    };
    State currentState = State::Disabled;
    
    GLFWwindow* mainContextWindow = nullptr;
    
    ImageViewerWindow imageViewerWindow;
    PointerOverlayWindow pointerOverlayWindow;
    GrabScreenAreaWindow grabScreenWindow;
    
    DisplayLinkTimer displayLinkTimer;
    KeyboardMonitor keyboardMonitor;
    
    OverlayTriggerEventDetector overlayTriggerDetector;
    
    bool appFocusWasEnabled = false;
    
    void onDisplayLinkRefresh ()
    {
        bool appFocusRequested = false;
        
        switch (currentState)
        {
            case State::Disabled:
            {
                appFocusRequested = false;
                break;
            }
                
            case State::GrabScreen:
            {
                appFocusRequested = false;
                grabScreenWindow.runOnce();
                if (grabScreenWindow.grabbingFinished())
                {
                    if (grabScreenWindow.grabbedData().isValid)
                    {
                        // FIXME: start ImageViewer
                    }
                    else
                    {
                        currentState = State::Disabled;
                    }
                }
                break;
            }
            
            case State::ImageViewer:
            {
                if (imageViewerWindow.isEnabled())
                {
                    appFocusRequested = true;
                    imageViewerWindow.runOnce();
                }
                else
                {
                    appFocusRequested = false;
                    currentState = State::Disabled;
                }
                break;
            }
        }
        
        if (appFocusRequested != appFocusWasEnabled)
        {
            dl::setAppFocusEnabled (appFocusRequested);
            appFocusWasEnabled = appFocusRequested;
        }
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
    impl->grabScreenWindow.shutdown();
    
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
    impl->grabScreenWindow.initialize(impl->mainContextWindow);
    
    impl->displayLinkTimer.setCallback([this]() {
        impl->onDisplayLinkRefresh();
    });
    
    //    impl->keyboardMonitor.setKeyboardCtrlAltCmdFlagsCallback ([this](bool enabled) {
    //        impl->overlayTriggerDetector.onCtrlAltCmdFlagsChanged(enabled);
    //        impl->pointerOverlayWindow.setEnabled(impl->overlayTriggerDetector.isEnabled());
    //    });
    
    impl->keyboardMonitor.setKeyboardCtrlAltCmdSpaceCallback([this]() {
        bool couldGrab = impl->grabScreenWindow.startGrabbing();
        if (couldGrab)
        {
            impl->currentState = Impl::State::GrabScreen;
        }
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
