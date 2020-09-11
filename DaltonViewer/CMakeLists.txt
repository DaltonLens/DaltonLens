cmake_minimum_required(VERSION 3.5.1)

project(DaltonViewer)

include(ExternalProject)

ExternalProject_Add(glfw-deps
  PREFIX glfw
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/glfw"
  CMAKE_CACHE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/glfw
)

ExternalProject_Add(glew-deps
  PREFIX glew
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/glew/build/cmake"
  CMAKE_CACHE_ARGS
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/glew
)

add_library(glfw3 STATIC IMPORTED)
set_target_properties(glfw3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glfw/lib/libglfw3.a)
include_directories(glfw3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glfw/include)

add_library(GLEW STATIC IMPORTED)
set_target_properties(GLEW PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glew/lib/libGLEW.a)
include_directories(glfw3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glew/include)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
    $<$<CONFIG:Debug>:-O3>
)

set (IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/imgui")
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/examples)

set (dlv_SOURCES 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/examples/
    ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
    
    main.cpp
)

add_executable(dlv ${dlv_SOURCES})
add_dependencies(dlv glfw-deps glew-deps)
target_compile_definitions(dlv PRIVATE -DGL_SILENCE_DEPRECATION=1)
target_link_libraries(dlv glfw3 GLEW)
target_link_libraries(dlv
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework AppKit"
    "-framework CoreData"
    "-framework IOKit"
    "-framework Foundation"
)
