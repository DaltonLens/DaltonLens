add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W=1)

set (deps_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp

    ${DEPS_DIR}/nativefiledialog-extended/src/include/nfd.h
    ${DEPS_DIR}/nativefiledialog-extended/src/include/nfd.hpp

    ${DEPS_DIR}/cppuserprefs/CppUserPrefs.h
    ${DEPS_DIR}/cppuserprefs/tortellini.hh
)

if (APPLE)
    set (daltonGUI_platform_specific_sources 
         PlatformSpecific_macOS.cpp
         ${DEPS_DIR}/nativefiledialog-extended/src/nfd_cocoa.m
    )
elseif (UNIX)
    set (daltonGUI_platform_specific_sources 
        PlatformSpecific_linux.cpp
        ${DEPS_DIR}/xdo/xdo_mini.h
        ${DEPS_DIR}/xdo/xdo_mini.cpp
        ${DEPS_DIR}/nativefiledialog-extended/src/nfd_gtk.cpp
    )
elseif (WIN32)
    set (daltonGUI_platform_specific_sources 
        PlatformSpecific_windows.cpp
        ${DEPS_DIR}/nativefiledialog-extended/src/nfd_win.cpp
    )    
endif()

set (daltonGUI_SOURCES 
    ${deps_SOURCES}
    ${daltonGUI_platform_specific_sources}
    
    # Binary resource.
    DaltonLensIcon_xxd.cpp

    PlatformSpecific.h
    DaltonLensGUI.cpp
    DaltonLensGUI.h
    DaltonLensIcon.h
    DaltonLensIcon.cpp
    DaltonLensPrefs.cpp
    DaltonLensPrefs.h
    # DaltonLensGUI_macOS.h
    GrabScreenAreaWindow.cpp
    GrabScreenAreaWindow.h
    ImageCursorOverlay.h
    ImageCursorOverlay.cpp
    GLFWUtils.h
    GLFWUtils.cpp
    HighlightSimilarColor.h
    HighlightSimilarColor.cpp
    ImguiGLFWWindow.h
    ImguiGLFWWindow.cpp
    HelpWindow.cpp
    HelpWindow.h
    ImageViewerControlsWindow.cpp
    ImageViewerControlsWindow.h
    ImageViewerController.h
    ImageViewerWindow.cpp
    ImageViewerWindow.h
    ImageViewer.cpp
    ImageViewer.h
    ImguiUtils.h
)

add_library(daltonGUI ${daltonGUI_SOURCES})
add_dependencies(daltonGUI glfw-deps clip-deps)

target_include_directories(daltonGUI PRIVATE 
    "${CMAKE_SOURCE_DIR}/DaltonLens/macOS/MASShortcut"
    ${DEPS_DIR}/nativefiledialog-extended/src/include
    ${DEPS_DIR}/cppuserprefs
)

target_include_directories(daltonGUI PUBLIC
    ${DEPS_DIR}/fonts
)

if (UNIX AND NOT APPLE)
    target_include_directories(daltonGUI PRIVATE ${APPINDICATOR_INCLUDE_DIRS})
endif()

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

set(INKSCAPE_WIN32 "C:\\Program Files\\Inkscape\\bin\\inkscape.exe")
set(XXD_WIN32 "${CMAKE_SOURCE_DIR}/resources/xxd.exe")
if (WIN32 AND EXISTS ${INKSCAPE_WIN32})
    add_custom_target(HelpResources 
        "${INKSCAPE_WIN32}" "resources/DaltonLens_Help.svg" --export-width=575 --export-filename="resources/DaltonLens_Help_1x.png"
        COMMAND "${INKSCAPE_WIN32}" "resources/DaltonLens_Help.svg" --export-width=1150 --export-filename="resources/DaltonLens_Help_2x.png"
        COMMAND "${INKSCAPE_WIN32}" "resources/DaltonLens_Help_macOS.svg" --export-width=1150 --export-filename="resources/DaltonLens_Help_macOS_1x.png"
        COMMAND "${INKSCAPE_WIN32}" "resources/DaltonLens_Help_macOS.svg" --export-width=1150 --export-filename="resources/DaltonLens_Help_macOS_2x.png"
        COMMAND "${XXD_WIN32}" -i "resources/DaltonLens_Help_1x.png" "DaltonGUI/DaltonLensHelp_1x_resource.inc"
        COMMAND "${XXD_WIN32}" -i "resources/DaltonLens_Help_2x.png" "DaltonGUI/DaltonLensHelp_2x_resource.inc"
        COMMAND "${XXD_WIN32}" -i "resources/DaltonLens_Help_macOS_1x.png" "DaltonGUI/DaltonLensHelp_macOS_1x_resource.inc"
        COMMAND "${XXD_WIN32}" -i "resources/DaltonLens_Help_macOS_2x.png" "DaltonGUI/DaltonLensHelp_macOS_2x_resource.inc"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
