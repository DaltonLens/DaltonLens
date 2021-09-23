
// Code adapted from the excellent xdotool
// https://github.com/jordansissel/xdotool

// Copyright (c) 2007, 2008, 2009: Jordan Sissel.
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Jordan Sissel nor the names of its contributors
//       may be used to endorse or promote products derived from this software
//       without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY JORDAN SISSEL ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL JORDAN SISSEL BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "xdo_mini.h"

#include <cstdio>
#include <memory>
#include <cstring>

#define XDO_ERROR 1
#define XDO_SUCCESS 0

namespace
{

int _xdo_ewmh_is_supported(Display *display, const char *feature)
{
    Atom type = 0;
    long nitems = 0L;
    int size = 0;
    Atom *results = NULL;
    long i = 0;

    Window root;
    Atom request;
    Atom feature_atom;

    request = XInternAtom(display, "_NET_SUPPORTED", False);
    feature_atom = XInternAtom(display, feature, False);
    root = XDefaultRootWindow(display);

    results = (Atom *)xdo_get_window_property_by_atom(display, root, request, &nitems, &type, &size);
    for (i = 0L; i < nitems; i++)
    {
        if (results[i] == feature_atom)
        {
            free(results);
            return True;
        }
    }
    free(results);

    return False;
}

int _is_success(const char *funcname, int code) 
{
  /* Nonzero is failure. */
  if (code != 0)
    fprintf(stderr, "%s failed (code=%d)\n", funcname, code);
  return code;
}

} // anonymous

unsigned char *xdo_get_window_property_by_atom(Display *display, Window window, Atom atom,
                                               long *nitems, Atom *type, int *size)
{
    Atom actual_type;
    int actual_format;
    unsigned long _nitems;
    /*unsigned long nbytes;*/
    unsigned long bytes_after; /* unused */
    unsigned char *prop;
    int status;

    status = XGetWindowProperty(display, window, atom, 0, (~0L),
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &_nitems, &bytes_after,
                                &prop);
    if (status == BadWindow)
    {
        fprintf(stderr, "window id # 0x%lx does not exists!", window);
        return NULL;
    }
    if (status != Success)
    {
        fprintf(stderr, "XGetWindowProperty failed!");
        return NULL;
    }

    /*
   *if (actual_format == 32)
   *  nbytes = sizeof(long);
   *else if (actual_format == 16)
   *  nbytes = sizeof(short);
   *else if (actual_format == 8)
   *  nbytes = 1;
   *else if (actual_format == 0)
   *  nbytes = 0;
   */

    if (nitems != NULL)
    {
        *nitems = _nitems;
    }

    if (type != NULL)
    {
        *type = actual_type;
    }

    if (size != NULL)
    {
        *size = actual_format;
    }
    return prop;
}

/* Default to -1, initialize it when we need it */
static Atom atom_NET_WM_PID = -1;
static Atom atom_NET_WM_NAME = -1;
static Atom atom_WM_NAME = -1;
static Atom atom_STRING = -1;
static Atom atom_UTF8_STRING = -1;

int xdo_get_window_name(Display *display,
                        Window window,
                        unsigned char **name_ret,
                        int *name_len_ret,
                        int *name_type)
{
    if (atom_NET_WM_NAME == (Atom)-1)
    {
        atom_NET_WM_NAME = XInternAtom(display, "_NET_WM_NAME", False);
    }
    if (atom_WM_NAME == (Atom)-1)
    {
        atom_WM_NAME = XInternAtom(display, "WM_NAME", False);
    }
    if (atom_STRING == (Atom)-1)
    {
        atom_STRING = XInternAtom(display, "STRING", False);
    }
    if (atom_UTF8_STRING == (Atom)-1)
    {
        atom_UTF8_STRING = XInternAtom(display, "UTF8_STRING", False);
    }

    Atom type;
    int size;
    long nitems;

    /**
   * http://standards.freedesktop.org/wm-spec/1.3/ar01s05.html
   * Prefer _NET_WM_NAME if available, otherwise use WM_NAME
   * If no WM_NAME, set name_ret to NULL and set len to 0
   */

    *name_ret = xdo_get_window_property_by_atom(display, window, atom_NET_WM_NAME, &nitems,
                                                &type, &size);
    if (nitems == 0)
    {
        *name_ret = xdo_get_window_property_by_atom(display, window, atom_WM_NAME, &nitems,
                                                    &type, &size);
    }
    *name_len_ret = nitems;
    *name_type = type;

    return 0;
}

int xdo_get_pid_window(Display *display, Window window)
{
    Atom type;
    int size;
    long nitems;
    unsigned char *data;
    int window_pid = 0;

    if (atom_NET_WM_PID == (Atom)-1)
    {
        atom_NET_WM_PID = XInternAtom(display, "_NET_WM_PID", False);
    }

    data = xdo_get_window_property_by_atom(display, window, atom_NET_WM_PID, &nitems, &type, &size);

    if (nitems > 0)
    {
        /* The data itself is unsigned long, but everyone uses int as pid values */
        window_pid = (int)*((unsigned long *)data);
    }
    free(data);

    return window_pid;
}

int xdo_set_desktop_for_window(Display *display, Window wid, long desktop)
{
    XEvent xev;
    int ret = 0;
    XWindowAttributes wattr;
    XGetWindowAttributes(display, wid, &wattr);

    if (_xdo_ewmh_is_supported(display, "_NET_WM_DESKTOP") == False)
    {
        fprintf(stderr,
                "Your windowmanager claims not to support _NET_WM_DESKTOP, "
                "so the attempt to change a window's desktop location was "
                "aborted.\n");
        return XDO_ERROR;
    }

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.display = display;
    xev.xclient.window = wid;
    xev.xclient.message_type = XInternAtom(display, "_NET_WM_DESKTOP", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = desktop;
    xev.xclient.data.l[1] = 2; /* indicate we are messaging from a pager */

    ret = XSendEvent(display, wattr.screen->root, False,
                     SubstructureNotifyMask | SubstructureRedirectMask,
                     &xev);

    return _is_success("XSendEvent[EWMH:_NET_WM_DESKTOP]", ret == 0);
}
