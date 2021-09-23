//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <DaltonGUI/DaltonLensGUI.h>
#include <DaltonGUI/DaltonLensIcon.h>

#include <Dalton/Utils.h>

#define TRAY_APPINDICATOR 1
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
        _iconAbsolutePath = dl::DaltonLensIcon::instance().absolutePngPathInTemporaryFolder();

        static tray_menu main_menu[] = {
            {.text = "Grab Screen Region", .checked = -1, .cb = grabScreen_cb, .context = this },
            {.text = "-"},
            {.text = "Help", .checked = -1, .cb = help_cb, .context = this },
            {.text = "Quit", .checked = -1, .cb = quit_cb, .context = this },
            {.text = NULL} 
        };

        _tray = tray{
            .icon = _iconAbsolutePath.c_str(),
            .menu = main_menu
        };

        tray_init(&_tray);

        _dlGui.initialize ();
    }

    void run ()
    {
        bool shouldExit = false;
        dl::RateLimit rateLimit;
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

    void onQuit()
    {
        tray_exit ();
        _dlGui.shutdown ();
    }

    void onHelp()
    {
        _dlGui.helpRequested();
    }

    static void grabScreen_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onGrabScreen(); }
    static void quit_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onQuit(); }
    static void help_cb(struct tray_menu *item) { reinterpret_cast<DaltonSystemTrayApp*>(item->context)->onHelp(); }

private:
    tray _tray;
    dl::DaltonLensGUI _dlGui;
    std::string _iconAbsolutePath;
};

int main ()
{
    DaltonSystemTrayApp app;
    app.run ();
    return 0;
}
