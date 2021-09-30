
if (WIN32)
    set (DaltonLens_RESOURCES DaltonLens.rc)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

add_executable(daltonlens WIN32 main.cpp ${DaltonLens_RESOURCES})

if (UNIX)
    # App indicator includes the required gtk3 headers
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(APPINDICATOR REQUIRED appindicator3-0.1)
endif()

target_include_directories(daltonlens PRIVATE ${APPINDICATOR_INCLUDE_DIRS})

target_link_libraries(daltonlens
    daltonGUI
    ${APPINDICATOR_LIBRARIES}
)
