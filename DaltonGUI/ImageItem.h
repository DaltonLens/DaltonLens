//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/Image.h>
#include <Dalton/OpenGL.h>

#include <string>

namespace dl
{

// Image and its associated data.
struct ImageItem
{
    std::string imagePath;
    dl::ImageSRGBA im;
    dl::ImageSRGBA aliasedIm;
    GLTexture gpuTexture;
    GLTexture gpuAliasedTexture;
};

} // dl