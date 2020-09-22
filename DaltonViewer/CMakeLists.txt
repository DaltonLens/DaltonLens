set (dlv_SOURCES 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/examples/
    ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c
    ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.h
    
    DaltonViewerLib.h
    DaltonViewerLib.cpp
    main.cpp
)

add_executable(dlv ${dlv_SOURCES})
add_dependencies(dlv glfw-deps clip-deps)

target_compile_definitions(dlv PRIVATE -DGL_SILENCE_DEPRECATION=1)

target_link_libraries(dlv 
    glfw3
    clip
    dalton
)
target_link_libraries(dlv
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework AppKit"
    "-framework CoreData"
    "-framework IOKit"
    "-framework Foundation"
)
