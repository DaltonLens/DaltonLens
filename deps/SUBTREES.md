git subtree pull --prefix deps/glew https://github.com/nigels-com/glew.git master --squash
git subtree pull --prefix deps/glfw https://github.com/glfw/glfw.git master --squash
git subtree pull --prefix deps/imgui https://github.com/ocornut/imgui.git docking --squash

## Glew

Added src/*.c and include/* to the repository so we don't have to do the `make -C auto` everytime. Then `cmake build/cmake` should also just work on Windows.
