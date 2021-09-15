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
    
    bool
    readPngImage (const uint8_t* inputBuffer,
                  size_t         inputSize,
                  ImageSRGBA&    outputImage)
    {
        assert(false); // not implemented
        return false;
    }

    bool readPngImage (const std::string& inputFileName, ImageSRGBA& outputImage)
    {
        std::ifstream f (inputFileName.c_str(), std::ios::binary|std::ios::ate);
        
        if (!f.is_open())
        {
            dl_dbg ("Could not open the file %s: %s", inputFileName.c_str(), strerror(errno));
            return false;
        }
        
        std::ifstream::pos_type fileSizeInBytes = f.tellg();
        std::vector<char> result(fileSizeInBytes);
        f.seekg(0, std::ios::beg);
        f.read(reinterpret_cast<char*>(result.data()), fileSizeInBytes);        
        return readPngImage ((const uint8_t*)result.data(), fileSizeInBytes, outputImage);
    }
    
    bool writePngImage (const std::string& filePath, const ImageSRGBA& image)
    {
        assert(false); // not implemented
        return false;
    }
    
} // dl
