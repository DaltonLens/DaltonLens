![image](https://img.shields.io/github/workflow/status/DaltonLens/DaltonLens/CMake?label=Linux%20build%20%26%20tests)
![image](https://img.shields.io/github/workflow/status/DaltonLens/DaltonLens/Xcode%20-%20Build?label=macOS%20build)

# DaltonLens

DaltonLens is a utility to help colorblind people by providing color filters and highlighting tools. It is especially useful to read color-coded charts and plots. Here are the main features:

- Show a pointer over your screen to get information about the color under the mouse pointer, including its name.

- Grab a screen region and apply color filters on top of it. The current filters include the daltonize algorithm to transfer the color differences into color channels that are easy to distinguish.

- Highlight similar colors in the region. This is especially useful for charts and plots, and it uses adaptive filters to better handle anti-aliased line plots.

It is designed to be lightweight and very reactive by relying mostly on GPU processing.

## Getting started

The app is available in the [mac app store](https://apps.apple.com/us/app/dalton-lens/id1222737651) or you can build the source code on Linux.

Then you will see a menu tray icon showing that the program is running.
* You can select the `Grab Screen Region` menu item there, but the recommend way is to use the global shortcut `Ctrl+Alt+Cmd+Space` to activate it and get an overlay showing color information about the pixel under the mouse pointer.
* Then `q` or `escape` to exit the pointer overlay, or click and drag over a region to select it. You can also hit `Space` to automatically select the window under the cursor.
* Once in the image viewer window you can click on a pixel to highlight similar colors in the image.
* Use `Right-Click` to get a contextual menu and the list of available filters.
* The help menu item gives all the shortcuts. `Shift` is particularly useful to show the original image regardless of what mode you are currently in.

## Demo video & screenshots

[![Demo Video Preview](https://user-images.githubusercontent.com/541507/103801408-fbac2980-504d-11eb-91f2-912ca234381f.png)](https://user-images.githubusercontent.com/541507/103819922-a54de380-506b-11eb-8644-b35392fa0254.mp4)

![Apply Color Filters](https://user-images.githubusercontent.com/541507/103801564-2eeeb880-504e-11eb-9be1-246ad482b120.png)

![Highlight Similar Color](https://user-images.githubusercontent.com/541507/103801587-39a94d80-504e-11eb-8292-0ad6fb9f2727.png)

![Help Window](https://user-images.githubusercontent.com/541507/103465834-9b607380-4d3f-11eb-9edf-655f3268e4ef.png)

## Supported platforms

Currently only macOS Mojave (10.14) and newer are supported (including Apple Silicon), but Windows support is likely to come next.

## Building the source code

## Building on macOS

Open DaltonLens/macOS/DaltonLens.xcodeproj in Xcode. There are precompiled binaries for the dependencies, so it should just build.

To rebuild the dependencies (glfw and clip, even though only glfw is actually used), run the `deps/update_dependencies_prebuilt_binaries.sh` script. You will need `cmake` and the Xcode command line tools installed.

### Building on Linux

Just run cmake after installing the required dependencies. Mostly X11, but unfortunately also GTK via libappindicator to get the system tray icon. Hope to get rid of that dependency one day!

Here the apt-get command line tested with Kubuntu 20.04:
> sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libappindicator3-dev libxcb1-dev

## Donate

I use DaltonLens daily myself, so this donate button is mostly here to give you a chance to say thanks and make me enjoy the special taste of a free coffee :-) If it ever becomes significant I will use it to cover some hardware cost for deep learning as I'm planning to develop smarter color analysis algorithms for chart analysis.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=64SEC4LDVJXUA&item_name=Support+DaltonLens&currency_code=EUR)

## License: BSD (with 2 clauses)
