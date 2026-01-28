set(MACOS_CMAKE_SOURCES
    main.mm
)

# Create the .app bundle
add_executable(DaltonLens MACOSX_BUNDLE ${MACOS_CMAKE_SOURCES})

# Link to daltonGUI (which includes all the core functionality)
target_link_libraries(DaltonLens
    daltonGUI
    "-framework Cocoa"
    "-framework ServiceManagement"
)

# Enable ARC for Objective-C++
target_compile_options(DaltonLens PRIVATE -fobjc-arc)

# Configure Info.plist
set_target_properties(DaltonLens PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    MACOSX_BUNDLE_BUNDLE_NAME "DaltonLens"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_COMMIT}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.daltonlens.DaltonLens"
    MACOSX_BUNDLE_ICON_FILE "DaltonLens"
)

# Copy icon to Resources
set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/DaltonLens.icns")
if(EXISTS ${ICON_SOURCE})
    set_source_files_properties(${ICON_SOURCE} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(DaltonLens PRIVATE ${ICON_SOURCE})
endif()

# Copy menu bar icon as a resource
set(MENUBAR_ICON "${CMAKE_SOURCE_DIR}/DaltonLens/macOS/Assets.xcassets/DaltonLensIcon_32x32@1x.png")
if(EXISTS ${MENUBAR_ICON})
    add_custom_command(TARGET DaltonLens POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:DaltonLens>/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy "${MENUBAR_ICON}" "$<TARGET_BUNDLE_CONTENT_DIR:DaltonLens>/Resources/DaltonLensIcon_32x32.png"
        COMMENT "Copying menu bar icon"
    )
endif()

# Include directories
target_include_directories(DaltonLens PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Ensure dependencies are built first
add_dependencies(DaltonLens glfw-deps clip-deps)
