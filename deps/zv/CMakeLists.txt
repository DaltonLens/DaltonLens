add_executable(zv-client WIN32 main.cpp Client.h Client.cpp)
target_link_libraries(zv-client pthread)