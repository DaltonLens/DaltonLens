set (daltonGUI_SOURCES 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/examples/
    ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.h    
)

add_library(daltonGUI ${daltonGUI_SOURCES})
add_dependencies(daltonGUI glfw-deps clip-deps)

target_compile_definitions(daltonGUI PUBLIC -DGL_SILENCE_DEPRECATION=1)

target_link_libraries(daltonGUI 
    glfw3
    clip
    dalton
)

target_link_libraries(daltonGUI
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework AppKit"
    "-framework CoreData"
    "-framework IOKit"
    "-framework Foundation"
)
