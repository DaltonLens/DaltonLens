//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <DaltonGUI/DaltonLensGUI.h>

#include <Dalton/Utils.h>

#define TRAY_APPINDICATOR 1
#include <tray/tray.h>

#include <thread>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <unistd.h>

namespace fs = std::filesystem;

// Icon embedded as a binary resource to get a standalone binary.
extern unsigned char __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png[];
extern unsigned int __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png_len;

// Saves the icon resource to a temporary file to feed it to tray.
struct IconResource
{
    IconResource ()
    {
        // Include the uid to avoid issues with multiple users.
        _icon_path = fs::temp_directory_path() / (std::string("dalton_lens_tray_icon_") + std::to_string(getuid()) + ".png");
        {
            std::ofstream f (_icon_path, std::ofstream::binary | std::ofstream::trunc);
            f.write((const char*)__DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png, __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png_len);
            assert (f.good());
        }
    }

    ~IconResource ()
    {
        fs::remove(_icon_path);
    }

    std::string fullPath () const
    {
        return fs::absolute(_icon_path);
    }

private:
    fs::path _icon_path;
};

class DaltonSystemTrayApp
{
public:
    DaltonSystemTrayApp ()
    {
        _iconAbsolutePath = _iconResource.fullPath();

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
    IconResource _iconResource;
    std::string _iconAbsolutePath;
};

int main ()
{
    DaltonSystemTrayApp app;
    app.run ();
    return 0;
}
