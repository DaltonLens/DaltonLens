add_executable(daltonlens main.cpp)

# App indicator includes the required gtk3 headers
find_package(PkgConfig REQUIRED)
pkg_check_modules(APPINDICATOR REQUIRED appindicator3-0.1)

target_include_directories(daltonlens PRIVATE ${APPINDICATOR_INCLUDE_DIRS})

target_link_libraries(daltonlens
    daltonGUI
    X11
    ${APPINDICATOR_LIBRARIES}
)
