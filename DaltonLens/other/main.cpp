//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <DaltonGUI/DaltonLensGUI.h>
#include <DaltonGUI/DaltonLensIcon.h>
#include <DaltonGUI/PlatformSpecific.h>

#include <Dalton/Utils.h>
#include <Dalton/Platform.h>

#include <zv/Client.h>

#if PLATFORM_LINUX
# define TRAY_APPINDICATOR 1
#else
# define TRAY_WINAPI 1
# define NOMINMAX
#endif

#include <tray/tray.h>

#include <thread>
#include <iostream>
#include <fstream>
#include <cassert>

class DaltonSystemTrayApp
{
public:
    DaltonSystemTrayApp ()
    {
        _iconAbsolutePath = dl::DaltonLensIcon::instance().absoluteIconPath();

        // text, disabled, checked, cb, context, submenu
        static tray_menu main_menu[] = {
            { "Grab Screen Region", 0, -1, grabScreen_cb, this },
#if PLATFORM_WINDOWS
            { "-" },
            { "Launch at Startup", 0, _startupManager.isLaunchAtStartupEnabled() ? 1 : 0, launchAtStartup_cb, this },
#endif
            { "-" },
            { "Help", 0, -1, help_cb, this },
            { "Quit", 0, -1, quit_cb, this },
            { NULL}
        };

        _tray = tray{
            _iconAbsolutePath.c_str(), // .icon
            main_menu // .menu
        };

        _dlGui.initialize ();

        // Note: for some reason we need to initialize GTK AFTER dlGui, otherwise NFD won't work.
        tray_init(&_tray);
    }

    void run ()
    {
        bool shouldExit = false;
        dl::RateLimit rateLimit;

        int n = 0;

        while (!shouldExit)
        {
            shouldExit = tray_loop(false /* not blocking */) != 0;
            _dlGui.runOnce ();
            rateLimit.sleepIfNecessary (1 / 30.);
        }
    }

private:
    void onGrabScreen ()
    {
        _dlGui.toogleGrabScreenArea();
    }

    void onLaunchAtStartup (struct tray_menu *item)
    {
        item->checked = !item->checked;
        _startupManager.setLaunchAtStartupEnabled (item->checked);
        tray_update (&_tray);
    }

    void onQuit()
    {
        tray_exit ();
        _dlGui.shutdown ();
    }

    void onHelp()
    {
        _dlGui.helpRequested();
    }
    
    static void launchAtStartup_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onLaunchAtStartup(item); }
    static void grabScreen_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onGrabScreen(); }
    static void quit_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onQuit(); }
    static void help_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onHelp(); }

private:
    tray _tray;
    dl::DaltonLensGUI _dlGui;
    std::string _iconAbsolutePath;
    dl::StartupManager _startupManager;
};

#include <Dalton/DeepAlias.h>

int main ()
{
    // dl::DeepAlias alias;
    // dl::ImageSRGBA input;
    // dl::readPngImage("/home/nb/Perso/DaltonLensPrivate/charts/inputs/tests/mpl-generated/img-00001.rendered.png", input);
    // dl::ImageSRGBA output = alias.undoAntiAliasing (input);
    // output = alias.undoAntiAliasing (input);

#ifndef NDEBUG
    zv::connect ();
#endif
    // zv::logImageRGBA ("input", input.rawBytes(), input.width (), input.height (), input.bytesPerRow ());
    // zv::logImageRGBA ("output", output.rawBytes(), output.width (), output.height (), output.bytesPerRow ());
    // zv::waitUntilDisconnected ();

    // dl::writePngImage("output.png", output);
    // return 0;
    
    DaltonSystemTrayApp app;
    app.run ();
    return 0;
}
