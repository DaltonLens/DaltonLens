//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Image.h"
#include "Utils.h"

#include <ImageIO/CGImageDestination.h>
#include <CoreGraphics/CGDataProvider.h>
#include <CoreFoundation/CFDictionary.h>

#include <fstream>
#include <vector>

namespace dl {
    
    bool
    readPngImage (const uint8_t* inputBuffer,
                  size_t         inputSize,
                  ImageSRGBA&    outputImage)
    {
        assert(0 != inputBuffer);
        assert(0 != inputSize);
        size_t outputWidth = 0;
        size_t outputHeight = 0;
        
        // TODO: This function outputs an RGB buffer, but we really shouldn't bother converting to RGB at all.
        // We could choose a faster path like direct YUV display (from JPEG's internal YUV) or at least
        // retain the premultiplied alpha that comes out of the regular iOS/OS X JPEG decode.
        
        // FIXME: we need to add an autoreleasepool block here since it is running in a background
        // thread. See https://developer.apple.com/library/ios/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/mmAutoreleasePools.html#//apple_ref/doc/uid/20000047-1041876
        
        // TODO: We can cache the buffer to avoid malloc/free.
        
        CFDataRef data  = CFDataCreateWithBytesNoCopy(NULL, inputBuffer, inputSize, kCFAllocatorNull);
        
        CFStringRef       imageSourceCreateKeys[1];
        CFTypeRef         imageSourceCreateValues[1];
        CFDictionaryRef   imageSourceCreateOptions;
        
        imageSourceCreateKeys[0] = kCGImageSourceShouldCache;
        imageSourceCreateValues[0] = (CFTypeRef)kCFBooleanFalse;
        imageSourceCreateOptions = CFDictionaryCreate(
                                                      kCFAllocatorDefault,
                                                      reinterpret_cast<const void**>(imageSourceCreateKeys),
                                                      reinterpret_cast<const void**>(imageSourceCreateValues),
                                                      1,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks
                                                      );
        
        CGImageSourceRef imageSource = CGImageSourceCreateWithData (data, imageSourceCreateOptions);
        
        //read image
        CGImageRef image = CGImageSourceCreateImageAtIndex(imageSource, 0, NULL);
        
        CFRelease(imageSourceCreateOptions);
        
        outputWidth = CGImageGetWidth(image);
        outputHeight = CGImageGetHeight(image);
        
        static const size_t maximumReasonableSize = 1024 * 32;
        
        assert(outputWidth < maximumReasonableSize);
        assert(outputHeight < maximumReasonableSize);
        
        CGColorSpaceRef colorSpace    = 0;
        size_t          bytesPerPixel = 0;
        CGBitmapInfo    bitmapInfo    = 0;
        
        colorSpace = CGColorSpaceCreateDeviceRGB();
        bytesPerPixel = 4;
        bitmapInfo = kCGImageAlphaNoneSkipLast;
        
        // Set the outputSize to the RGBA buffer size.
        outputImage.ensureAllocatedBufferForSize((int)outputWidth, (int)outputHeight);
        
        CGContextRef context = CGBitmapContextCreate(
                                                     outputImage.data(),
                                                     outputWidth,
                                                     outputHeight,
                                                     8,
                                                     outputImage.bytesPerRow(),
                                                     colorSpace,
                                                     bitmapInfo
                                                     );
        
        if(context == 0)
        {
            CFRelease(data);
            CFRelease(image);
            CFRelease(imageSource);
            CFRelease(colorSpace);
            
            return false;
        }
        
        CGContextDrawImage(context, CGRectMake(0, 0, outputWidth, outputHeight), image);
        
        CFRelease(image);
        
        CGColorSpaceRelease(colorSpace);
        CGContextRelease(context);
        
        CFRelease(imageSource);
        CFRelease(data);
        
        return true;
    }

    bool readPngImage (const std::string& inputFileName,
                       ImageSRGBA& outputImage)
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
        assert(image.hasData());
        CGDataProviderRef dataProvider = CGDataProviderCreateWithData(NULL, image.data(), image.sizeInBytes(), NULL);
        
        if (!dataProvider)
            return false;
        
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        const size_t bytesPerPixel = 4;
        
        if (!colorSpace)
        {
            CFRelease(dataProvider);
            return false;
        }
        
        CGImageRef cgImage = CGImageCreate(image.width(),
                                           image.height(),
                                           8,
                                           8 * bytesPerPixel,
                                           image.bytesPerRow(),
                                           colorSpace,
                                           kCGImageAlphaNoneSkipLast,
                                           dataProvider,
                                           NULL,
                                           false,
                                           kCGRenderingIntentDefault);
        
        if (!cgImage)
        {
            CGDataProviderRelease(dataProvider);
            CGColorSpaceRelease(colorSpace);
            return false;
        }
        
        CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
        
        if (0 == data)
        {
            CGDataProviderRelease(dataProvider);
            CGColorSpaceRelease(colorSpace);
            CGImageRelease(cgImage);
            
            return false;
        }
        
        CFStringRef typeIdentifier = CFSTR("public.png");;
        CFDictionaryRef properties = 0;
        
        CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, typeIdentifier, 1, NULL);
        
        if (!destination)
        {
            CGDataProviderRelease(dataProvider);
            CGColorSpaceRelease(colorSpace);
            CGImageRelease(cgImage);
            CFRelease(data);
            
            if (properties)
                CFRelease(properties);
            
            return false;
        }
        
        CGImageDestinationAddImage(destination, cgImage, properties);
        
        if (0 != properties)
            CFRelease(properties);
        
        if (!CGImageDestinationFinalize(destination))
        {
            CGDataProviderRelease(dataProvider);
            CGColorSpaceRelease(colorSpace);
            CGImageRelease(cgImage);
            CFRelease(data);
            CFRelease(destination);
            
            return false;
        }
        
        const size_t   compressedSize  = CFDataGetLength(data);
        const uint8_t* compressedBytes = CFDataGetBytePtr(data);
        
        {
            std::ofstream f (filePath.c_str());
            assert (f.is_open());
            f.write ((const char*)compressedBytes, compressedSize);
            assert (f.good());
        }
        
        CFRelease(data);
        CFRelease(destination);
        CGImageRelease(cgImage);
        CGColorSpaceRelease(colorSpace);
        CGDataProviderRelease(dataProvider);
        return true;
    }

    
} // dl
