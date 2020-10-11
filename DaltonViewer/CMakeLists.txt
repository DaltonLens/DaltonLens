set (dlv_SOURCES 
    main.cpp
)

add_executable(dlv ${dlv_SOURCES})

target_compile_definitions(dlv PRIVATE -DGL_SILENCE_DEPRECATION=1)

target_link_libraries(dlv 
    daltonGUI
)
