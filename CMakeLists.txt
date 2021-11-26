cmake_minimum_required(VERSION 3.5.1)

project(DaltonLens)
set(PROJECT_VERSION_MAJOR 2)
set(PROJECT_VERSION_MINOR 4)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# Get the current commit as a version variable.
# Make sure that we'll re-run configure if the commit changes.
# https://cmake.org/pipermail/cmake/2018-October/068389.html
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --short HEAD
         RESULT_VARIABLE PROJECT_VERSION_COMMIT_RESULT
         OUTPUT_VARIABLE PROJECT_VERSION_COMMIT
         OUTPUT_STRIP_TRAILING_WHITESPACE)
set_property(GLOBAL APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/index")

include(ExternalProject)

configure_file(${CMAKE_SOURCE_DIR}/Dalton/DaltonGeneratedConfig.h.in ${PROJECT_BINARY_DIR}/DaltonGeneratedConfig.h)
include_directories(${PROJECT_BINARY_DIR})

set(PRECOMPILED_FOLDER "${CMAKE_SOURCE_DIR}/deps/prebuilt/${CMAKE_SYSTEM_NAME}")

# Hack to accept multiple archs separated by semicolon.
# https://discourse.cmake.org/t/how-to-pass-cmake-osx-architectures-to-externalproject-add/2262/3
set(semicolon_smuggle "-+-")
string(REPLACE ";" "${semicolon_smuggle}" cmake_osx_archs_special_separator "${CMAKE_OSX_ARCHITECTURES}")

# DebugFast does not exist for the deps, so need to set it to Debug.
if (CMAKE_BUILD_TYPE STREQUAL DebugFast OR CMAKE_BUILD_TYPE STREQUAL DEBUGFAST)
  set (CMAKE_DEPS_CONFIG Debug)
else()
  set (CMAKE_DEPS_CONFIG ${CMAKE_BUILD_TYPE})
endif()

ExternalProject_Add(glfw-deps
  LIST_SEPARATOR "${semicolon_smuggle}"
  PREFIX glfw
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/glfw"
  CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_DEPS_CONFIG}
        -DCMAKE_OSX_ARCHITECTURES=${cmake_osx_archs_special_separator}
        -DCMAKE_INSTALL_PREFIX:PATH=${PRECOMPILED_FOLDER}/glfw
        -DGLFW_BUILD_DOCS=OFF
        -DGLFW_BUILD_EXAMPLES=OFF
        -DGLFW_BUILD_TESTS=OFF
)

set(GLFW_LIBRARY_FILENAME "${CMAKE_STATIC_LIBRARY_PREFIX}glfw3${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_library(glfw3 STATIC IMPORTED)
set_target_properties(glfw3 PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/glfw/lib/${GLFW_LIBRARY_FILENAME})
include_directories(glfw3 PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/glfw/include)

if (WIN32)
  target_link_libraries(glfw3 INTERFACE opengl32)
endif()

set(CLIP_LIBRARY_FILENAME "${CMAKE_STATIC_LIBRARY_PREFIX}clip${CMAKE_STATIC_LIBRARY_SUFFIX}")
ExternalProject_Add(clip-deps
  LIST_SEPARATOR "${semicolon_smuggle}"
  PREFIX clip
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/clip"
  CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_DEPS_CONFIG}
        -DCMAKE_OSX_ARCHITECTURES=${cmake_osx_archs_special_separator}
        -DCMAKE_INSTALL_PREFIX:PATH=${PRECOMPILED_FOLDER}/clip
        -DCLIP_EXAMPLES=OFF
        -DCLIP_TESTS=OFF
        -DCLIP_X11_WITH_PNG=OFF
  # Now handled in deps/clip/CMakeLists.txt, otherwise this leads
  # to issues with Visual Studio 2019 as it cannot find an INSTALL
  # target in the external project.
  # INSTALL_COMMAND 
  #      cmake -E make_directory "${PRECOMPILED_FOLDER}/clip/lib/"
  #      COMMAND cmake -E copy "<BINARY_DIR>/${CLIP_LIBRARY_FILENAME}" "${PRECOMPILED_FOLDER}/clip/lib/"
)

add_library(clip STATIC IMPORTED)
set_target_properties(clip PROPERTIES IMPORTED_LOCATION ${PRECOMPILED_FOLDER}/clip/lib/${CLIP_LIBRARY_FILENAME})
include_directories(clip PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/deps)
if (WIN32)
  target_link_libraries(clip INTERFACE Shlwapi)
endif()

# Find appindicator on Unix for DaltonLens, used by NFD with DaltonGUI 
if (UNIX AND NOT APPLE)
    # App indicator includes the required gtk3 headers
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(APPINDICATOR REQUIRED appindicator3-0.1)
endif()

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

set(CMAKE_CXX_FLAGS_DEBUGFAST "${CMAKE_CXX_FLAGS_DEBUG} -Os" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_DEBUGFAST "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUGFAST "${CMAKE_EXE_LINKER_FLAGS_DEBUG}" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGFAST "${CMAKE_SHARED_LINKER_FLAGS_DEBUG}" CACHE STRING "" FORCE)
mark_as_advanced(
  CMAKE_CXX_FLAGS_DEBUGFAST
  CMAKE_C_FLAGS_DEBUGFAST
  CMAKE_EXE_LINKER_FLAGS_DEBUGFAST
  CMAKE_SHARED_LINKER_FLAGS_DEBUGFAST)
# Update the documentation string of CMAKE_BUILD_TYPE for GUIs

set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" "RelWithDebInfo" "DebugFast")

# add_compile_options(
#     $<$<CONFIG:Debug>:-Os>
# )

set (DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")

set (IMGUI_DIR "${DEPS_DIR}/imgui")
include_directories(
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends 
    ${DEPS_DIR}/gl3w
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/other)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/stb)

if (WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
endif()

add_subdirectory(Dalton)
add_subdirectory(DaltonGUI)

if (NOT APPLE)
  add_subdirectory(DaltonLens/other)
endif()

enable_testing()
add_subdirectory(tests)

# This does not work, it shows that the project needs migration and then it refuses to migrate it.
# So right now we have to insert the project manually in the Visual Studio 2019 solution.
#if (WIN32)
#  include_external_msproject(WindowsStore "${CMAKE_SOURCE_DIR}/WindowsStore/WindowsStore.wapproj")
#endif()
