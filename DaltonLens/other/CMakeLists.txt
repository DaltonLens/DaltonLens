
if (WIN32)
    set (DaltonLens_RESOURCES DaltonLens.rc DaltonLens_windows.manifest)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

add_executable(DaltonLens WIN32 main.cpp ${DaltonLens_RESOURCES})

add_executable(binary_to_compressed "${DEPS_DIR}/imgui/misc/fonts/binary_to_compressed_c.cpp")

if (UNIX)
    # App indicator includes the required gtk3 headers
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(APPINDICATOR REQUIRED appindicator3-0.1)
endif()

target_include_directories(DaltonLens PRIVATE ${APPINDICATOR_INCLUDE_DIRS})

target_link_libraries(DaltonLens
    daltonGUI
    ${APPINDICATOR_LIBRARIES}
)

if (WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT applications)

    install(TARGETS DaltonLens 
         DESTINATION .
         COMPONENT applications
    )
    
    set(CPACK_PACKAGE_NAME "DaltonLens")
    set(CPACK_PACKAGE_VENDOR "Nicolas Burrus")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DaltonLens is a utility to help colorblind people")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    if ("${PROJECT_VERSION_PATCH}" STREQUAL "")
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
    else()
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
    endif()
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "DaltonLens")
    
    set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "DaltonLens")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}\\\\DaltonLens\\\\other\\\\daltonlens.ico")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/DaltonLens/other/daltonlens.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/DaltonLens/other/daltonlens.ico")
    # set(CPACK_NSIS_MENU_LINKS "programs\\DaltonLens" "daltonlens")

    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "DaltonLens.exe")

    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\DaltonLens.lnk' '$INSTDIR\\\\DaltonLens.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\DaltonLens.lnk'"
    )

    # Remove the auto startup that may exist. ClearErrors since it may not exist and that's fine.
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "
        DeleteRegValue HKCU 'SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Run' 'DaltonLens'
        ClearErrors
    ")

    include(CPack)
elseif (UNIX)
    install(TARGETS DaltonLens 
         DESTINATION bin
    )

    # set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
    set(CPACK_GENERATOR "TGZ")

    set(CPACK_PACKAGE_NAME "DaltonLens")
    set(CPACK_PACKAGE_VENDOR "Nicolas Burrus")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "DaltonLens is a utility to help colorblind people")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
    if ("${PROJECT_VERSION_PATCH}" STREQUAL "")
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
    else()
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
    endif()
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "DaltonLens")

    set(CPACK_PACKAGE_EXECUTABLES "DaltonLens")
    set(CPACK_PACKAGE_FILE_NAME "DaltonLens-${PROJECT_VERSION}-Linux-amd64")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/Copyrights.txt")
    set(CPACK_SOURCE_GENERATOR "ZIP")
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "DaltonLens-${PROJECT_VERSION}")
    set(CPACK_SOURCE_STRIP_FILES "")
    set(CPACK_SOURCE_IGNORE_FILES "build/")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/\.git/")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/\.gitattributes")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/\.gitignore")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/\.github/")
    list(APPEND CPACK_SOURCE_IGNORE_FILES "/\.ipynb_checkpoints/")    
    set(CPACK_STRIP_FILES "bin/DaltonLens")

    include(CPack)
endif()
