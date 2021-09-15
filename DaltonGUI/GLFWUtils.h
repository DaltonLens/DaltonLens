#pragma once

#include <GLFW/glfw3.h>

namespace dl
{

// glfwShowWindow might change the location on Linux if the position was set before a glfwHideWindow. 
// Not sure if it's a bug or a misuse, but this hack will do.
struct GlfwMaintainWindowPosAfterScope
{
    GlfwMaintainWindowPosAfterScope(GLFWwindow* w) : w(w)
    {
        glfwGetWindowPos(w, &x, &y);
    }

    ~GlfwMaintainWindowPosAfterScope()
    {
        glfwSetWindowPos(w, x, y);
    }

private:
    GLFWwindow *w;
    int x, y;
};

} // dl
