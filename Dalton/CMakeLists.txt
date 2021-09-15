if (APPLE)
    set (dalton_platform_specific_sources Image_macOS.cpp)
else()
    set (dalton_platform_specific_sources Image_stb.cpp)
endif()

add_library(dalton 
    ColorConversion.cpp
    ColorConversion.h
    Common.h
    Image.h
    MathUtils.h
    Platform.h
    Simulator.cpp
    Simulator.h
    Utils.cpp
    Utils.h
    ${dalton_platform_specific_sources}
)
