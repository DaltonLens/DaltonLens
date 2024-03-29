//
// Copyright (c) 2021, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "DaltonLensIcon.h"

#include <Dalton/Platform.h>
#include <Dalton/Utils.h>

#include <stb/stb_image.h>

#include <filesystem>
#include <fstream>

#if PLATFORM_WINDOWS
# include <windows.h>
#endif

namespace fs = std::filesystem;

// Icon embedded as a binary resource to get a standalone binary.
extern unsigned char __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png[];
extern unsigned int __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png_len;

namespace dl
{

DaltonLensIcon& DaltonLensIcon::instance()
{
    static DaltonLensIcon icon;
    return icon;
}

DaltonLensIcon::DaltonLensIcon()
{
    /*
        This is not great, we currently have to the icon as a png file on disk.
        The reason is that libappindicator does not seem to accept a chunk of
        data from memory.
    */

    // We don't have filesystem on macOS without bumping the deployment target to 10.15 :-(
#if PLATFORM_LINUX
    // Include the uid to avoid issues with multiple users.
    fs::path icon_path = fs::temp_directory_path() / (std::string("dalton_lens_tray_icon_") + getUserId() + ".png");
    {
        std::ofstream f(icon_path, std::ofstream::binary | std::ofstream::trunc);
        f.write((const char *)__DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png, __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png_len);
        dl_assert (f.good(), "Could not save the temporary icon file.");
    }
        
    _absolute_png_path = fs::absolute(icon_path).u8string();

    int width = -1, height = -1, channels = -1;
    unsigned char *imageBuffer = stbi_load_from_memory(__DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png,
                                                       __DaltonLens_Assets_xcassets_DaltonLensIcon_32x32_1x_png_len,
                                                       &width, &height, &channels, 4);
    dl_assert (width == 32 && height == 32, "Unexpected image size");
    dl_assert (channels == 4, "RGBA expected!");

    size_t bufferSizeInBytes = width*height*channels;
    _rgba32x32.resize (bufferSizeInBytes);
    std::copy (imageBuffer, imageBuffer + bufferSizeInBytes, _rgba32x32.begin());

    stbi_image_free (imageBuffer);

    // Don't make us believe that we have valid data!
    if (channels != 4)
    {
        _rgba32x32.clear ();
    }
#endif
}

DaltonLensIcon::~DaltonLensIcon()
{
#if PLATFORM_LINUX
    fs::remove(_absolute_png_path);
#endif
}

std::string DaltonLensIcon::absoluteIconPath () const
{
#if PLATFORM_WINDOWS
    CHAR exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return exePath;
#elif PLATFORM_LINUX
    return _absolute_png_path;
#else
    return "";
#endif
}

} // dl
