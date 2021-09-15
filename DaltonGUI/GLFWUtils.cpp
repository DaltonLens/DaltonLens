//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "GLFWUtils.h"

#include <Dalton/Platform.h>

#if PLATFORM_LINUX
# define GLFW_EXPOSE_NATIVE_X11 1
# include <GLFW/glfw3native.h>
#endif

#include <Dalton/Platform.h>

namespace dl
{

void glfw_reliableBringToFront (GLFWwindow* w)
{
    glfwFocusWindow (w);
#if PLATFORM_LINUX
    Display* display = glfwGetX11Display ();
    Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", False );
    XEvent e;
    e.xclient.type = ClientMessage;
    e.xclient.message_type = net_active_window;
    e.xclient.display = display;
    e.xclient.window = glfwGetX11Window (w);
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2l;
    e.xclient.data.l[1] = CurrentTime;
    e.xclient.data.l[2] = None;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
#endif
}

} // dl