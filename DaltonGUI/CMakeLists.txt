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
    set (daltonGUI_platform_specific_sources CrossPlatformUtils_linux.cpp)
endif()

set (daltonGUI_SOURCES 
    ${deps_SOURCES}
    ${daltonGUI_platform_specific_sources}
    CrossPlatformUtils.h
    DaltonLensGUI.cpp
    DaltonLensGUI.h
    # DaltonLensGUI_macOS.h
    GrabScreenAreaWindow.cpp
    GrabScreenAreaWindow.h
    GLFWUtils.h
    Graphics.cpp
    Graphics.h
    HelpWindow.cpp
    HelpWindow.h
    ImageViewerWindow.cpp
    ImageViewerWindow.h
    ImGuiUtils.h
    PointerOverlayWindow.cpp
    PointerOverlayWindow.h
)

add_library(daltonGUI ${daltonGUI_SOURCES})
add_dependencies(daltonGUI glfw-deps clip-deps)

target_include_directories(daltonGUI PRIVATE "${CMAKE_SOURCE_DIR}/DaltonLens/macOS/MASShortcut")
target_compile_definitions(daltonGUI PUBLIC -DGL_SILENCE_DEPRECATION=1)
if (APPLE)
    target_compile_options(daltonGUI PUBLIC -x objective-c++)
endif()

target_link_libraries(daltonGUI 
    glfw3
    clip
    dalton
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
    )
endif()