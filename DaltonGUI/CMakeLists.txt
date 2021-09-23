add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W=1)

set (deps_SOURCES
    ${DEPS_DIR}/stb/stb_impl.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    
    ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.h
)    

if (APPLE)
    set (daltonGUI_platform_specific_sources CrossPlatformUtils_macOS.cpp)
else (UNIX)
    set (daltonGUI_platform_specific_sources 
        CrossPlatformUtils_linux.cpp
        ${DEPS_DIR}/xdo/xdo_mini.h
        ${DEPS_DIR}/xdo/xdo_mini.cpp
    )
endif()

set (daltonGUI_SOURCES 
    ${deps_SOURCES}
    ${daltonGUI_platform_specific_sources}
    
    # Binary resource.
    DaltonLensIcon_xxd.cpp

    CrossPlatformUtils.h
    DaltonLensGUI.cpp
    DaltonLensGUI.h
    DaltonLensIcon.h
    DaltonLensIcon.cpp
    # DaltonLensGUI_macOS.h
    GrabScreenAreaWindow.cpp
    GrabScreenAreaWindow.h
    GLFWUtils.h
    GLFWUtils.cpp
    Graphics.cpp
    Graphics.h
    HighlightSimilarColor.h
    HighlightSimilarColor.cpp
    ImguiGLFWWindow.h
    ImguiGLFWWindow.cpp
    HelpWindow.cpp
    HelpWindow.h
    ImageViewerControlsWindow.cpp
    ImageViewerControlsWindow.h
    ImageViewerObserver.h
    ImageViewerWindow.cpp
    ImageViewerWindow.h
    ImageViewer.cpp
    ImageViewer.h
    ImguiUtils.h
)

add_library(daltonGUI ${daltonGUI_SOURCES})
add_dependencies(daltonGUI glfw-deps clip-deps)

target_include_directories(daltonGUI PRIVATE "${CMAKE_SOURCE_DIR}/DaltonLens/macOS/MASShortcut")
target_compile_definitions(daltonGUI PUBLIC -DGL_SILENCE_DEPRECATION=1)
if (APPLE)
    target_compile_options(daltonGUI PUBLIC -x objective-c++)
endif()

# Gcc 3.8 requires that.
if (CMAKE_COMPILER_IS_GNUCXX)
    set (FS_LIBRARY stdc++fs)
endif()

target_link_libraries(daltonGUI 
    glfw3
    clip
    dalton
    ${FS_LIBRARY}
)

if (APPLE)
    target_link_libraries(daltonGUI
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework AppKit"
        "-framework CoreData"
        "-framework IOKit"
        "-framework Foundation"
    )
elseif (UNIX)
    target_link_libraries(daltonGUI
        GL
        dl # for gl3w
        pthread
        xcb # for clip. FIXME: should use a find_library
        X11
        ${APPINDICATOR_LIBRARIES}
    )
endif()
