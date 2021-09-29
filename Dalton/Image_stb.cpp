//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Image.h"
#include "Utils.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <fstream>
#include <vector>

namespace dl {
    
    bool readPngImage (const std::string& inputFileName, ImageSRGBA& outputImage)
    {
        int width = -1, height = -1, channels = -1;
        uint8_t* data = stbi_load(inputFileName.c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            return false;
        }

        outputImage.ensureAllocatedBufferForSize (width, height);
        outputImage.copyDataFrom (data, width*4, width, height);
        return true;
    }
    
    bool writePngImage (const std::string& filePath, const ImageSRGBA& image)
    {
        return stbi_write_png(filePath.c_str(), image.width(), image.height(), 4, image.data(), image.bytesPerRow());
    }
    
} // dl
