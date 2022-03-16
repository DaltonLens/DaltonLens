if (APPLE)
    set (dalton_platform_specific_sources Image_macOS.cpp)
else()
    set (dalton_platform_specific_sources Image_stb.cpp)
endif()

set (dalton_deps_sources
    ${DEPS_DIR}/gl3w/GL/gl3w.c
    ${DEPS_DIR}/gl3w/GL/gl3w.h
    ${DEPS_DIR}/stb/stb_impl.cpp
)

add_library(dalton 
    ColorConversion.cpp
    ColorConversion.h
    Filters.h
    Filters.cpp
    Image.h    
    MathUtils.h
    OpenGL.h
    OpenGL.cpp
    OpenGL_Shaders.h
    OpenGL_Shaders.cpp
    Platform.h
    Utils.cpp
    Utils.h
    ${dalton_platform_specific_sources}
    ${dalton_deps_sources}
)

add_dependencies(dalton glfw-deps clip-deps)

target_include_directories(dalton 
    PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(dalton 
    glfw3
    ${OpenCV_LIBS}
)
