[![CMake Build and Test](https://github.com/DaltonLens/DaltonLens/actions/workflows/cmake_build_and_test.yml/badge.svg)](https://github.com/DaltonLens/DaltonLens/actions/workflows/cmake_build_and_test.yml)
[![Xcode - Build](https://github.com/DaltonLens/DaltonLens/actions/workflows/xcode.yml/badge.svg)](https://github.com/DaltonLens/DaltonLens/actions/workflows/xcode.yml)

# DaltonLens

DaltonLens is a utility to help colorblind people by providing color filters and highlighting tools. It is especially useful to read color-coded charts and plots. Here are the main features:

- Runs as a tray program and always instantly available via the `CMD/Win + Alt + Ctrl + Space` global shortchut.

- Can show an overlay over your screen to get information about the color under the mouse pointer, including its name.

- Grab a screen region and apply color filters on top of it. The current filters include the daltonize algorithm to transfer the color differences into color channels that are easy to distinguish.

- Click on a pixel and highlight similar colors in the image. This is especially useful for charts and plots, and it uses adaptive filters to better handle anti-aliased line plots.

It is designed to be lightweight and very reactive by relying mostly on GPU processing.

## Getting started

* Windows / Linux: download the Windows installer or the Linux amd64 binary from the release section
* macOS: the app is available in the [mac app store](https://apps.apple.com/us/app/dalton-lens/id1222737651)

Then you will see a menu tray icon showing that the program is running.
* You can select the `Grab Screen Region` menu item there, but the recommend way is to use the global shortcut `Ctrl+Alt+Cmd/Win+Space` to activate it and get an overlay showing color information about the pixel under the mouse pointer.
* Then `q` or `escape` to exit the pointer overlay, or click and drag over a region to select it. You can also hit `Space` to automatically select the window under the cursor.
* Once in the image viewer window you can click on a pixel to highlight similar colors in the image.
* Use the controls window to switch the filter / configure the options
* The help menu item gives all the shortcuts. `Shift` is particularly useful to show the original image regardless of what mode you are currently in.

## Demo video & screenshots

[![Demo Video Preview](https://user-images.githubusercontent.com/541507/103801408-fbac2980-504d-11eb-91f2-912ca234381f.png)](https://user-images.githubusercontent.com/541507/137916903-208a7a7b-081f-4b5b-a7a7-1850f571e2ae.mov)

![Apply Color Filters](https://user-images.githubusercontent.com/541507/137916868-4649b7e9-049f-424d-8703-3ae14914a1bd.png)

![Help Window](https://user-images.githubusercontent.com/541507/137917928-b579584f-70cb-40c5-a494-26a8c7055f33.png)

## Supported platforms

* macOS: Mojave (10.14) and newer are supported (including Apple Silicon)
* Windows: only tested on Windows 10, but anything more recent than Windows 8.1 should work
* Linux: only tested on Ubuntu 20.04, but should work on any distribution that has gtk3 and libappindicator3.

## Building the source code

### Building on macOS

Open DaltonLens/macOS/DaltonLens.xcodeproj in Xcode. There are precompiled binaries for the dependencies, so it should just build.

To rebuild the dependencies (glfw and clip, even though only glfw is actually used), run the `deps/update_dependencies_prebuilt_binaries.sh` script. You will need `cmake` and the Xcode command line tools installed.

### Building on Linux

Just run cmake after installing the required dependencies. Mostly X11, but unfortunately also GTK via libappindicator to get the system tray icon. Hope to get rid of that dependency one day!

Here the apt-get command line tested with Kubuntu 20.04:
> sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libappindicator3-dev libxcb1-dev

### Building on Windows

Run cmake with the Visual Studio Express or Ninja generator.

### Dependencies

All the dependencies are embedded in the repository to avoid version hell, so **you don't need to install anything**. Still listing the dependencies here for awareness:

* [imgui](https://github.com/ocornut/imgui)
* [GLFW](https://www.glfw.org/)
* [gl3w](https://github.com/skaslev/gl3w)
* [stb image](https://github.com/nothings/stb)
* [tray](https://github.com/zserge/tray)
* [clip](https://github.com/dacap/clip)

## Donate

I use DaltonLens daily myself, so this donate button is mostly here to give you a chance to say thanks and make me enjoy the special taste of a free coffee :-) If it ever becomes significant I will use it to cover some hardware cost for deep learning as I'm planning to develop smarter color analysis algorithms for chart analysis.

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=64SEC4LDVJXUA&item_name=Support+DaltonLens&currency_code=EUR)

## License: BSD (with 2 clauses)
