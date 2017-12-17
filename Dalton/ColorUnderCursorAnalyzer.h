//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

typedef struct DLRGBAPixel
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} DLRGBAPixel;

@interface DLColorUnderCursorAnalyzer : NSObject

-(DLRGBAPixel)dominantColorInBuffer:(const uint8_t*)data
                            width:(size_t)width
                           height:(size_t)height
                      bytesPerRow:(size_t)bytesPerRow;

-(DLRGBAPixel)darkestColorInBuffer:(const uint8_t*)data
                           width:(size_t)width
                          height:(size_t)height
                     bytesPerRow:(size_t)bytesPerRow;

@end
