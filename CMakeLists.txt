cmake_minimum_required(VERSION 3.5.1)

project(DaltonLens)

include(ExternalProject)

set(PRECOMPILED_FOLDER "${CMAKE_SOURCE_DIR}/deps/prebuilt")

# Hack to accept multiple archs separated by semicolon.
# https://discourse.cmake.org/t/how-to-pass-cmake-osx-architectures-to-externalproject-add/2262/3
set(semicolon_smuggle "-+-")
string(REPLACE ";" "${semicolon_smuggle}" cmake_osx_archs_special_separator "${CMAKE_OSX_ARCHITECTURES}")

ExternalProject_Add(glfw-deps
  LIST_SEPARATOR "${semicolon_smuggle}"
  PREFIX glfw
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw"
  CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_OSX_ARCHITECTURES=${cmake_osx_archs_special_separator}
        -DCMAKE_INSTALL_PREFIX:PATH=${PRECOMPILED_FOLDER}/glfw
)

add_library(glfw3 STATIC IMPORTED)
set_target_properties(glfw3 PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/glfw/lib/libglfw3.a)
include_directories(glfw3 PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/glfw/include)

ExternalProject_Add(clip-deps
  LIST_SEPARATOR "${semicolon_smuggle}"
  PREFIX clip
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/clip"
  CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_OSX_ARCHITECTURES=${cmake_osx_archs_special_separator}
        -DCMAKE_INSTALL_PREFIX:PATH=${PRECOMPILED_FOLDER}/clip
  INSTALL_COMMAND 
        cmake -E make_directory "${PRECOMPILED_FOLDER}/clip/lib/"
        COMMAND cmake -E copy "<BINARY_DIR>/libclip.a" "${PRECOMPILED_FOLDER}/clip/lib/"
)

add_library(clip STATIC IMPORTED)
set_target_properties(clip PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/clip/lib/libclip.a)
include_directories(clip PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# ExternalProject_Add(glew-deps
#   PREFIX glew
#   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/glew/build/cmake"
#   CMAKE_ARGS
#         -DCMAKE_BUILD_TYPE=Release
#         -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/glew
# )

# add_library(GLEW STATIC IMPORTED)
# set_target_properties(GLEW PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glew/lib/libGLEW.a)
# include_directories(glfw3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/glew/include)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(
    $<$<CONFIG:Debug>:-Os>
)

set (IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui")
include_directories(
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/examples 
    ${IMGUI_DIR}/examples/libs/gl3w
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/other)

add_subdirectory(Dalton)
add_subdirectory(DaltonGUI)

