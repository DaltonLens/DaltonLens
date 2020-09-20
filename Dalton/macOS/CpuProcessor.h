//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

#include "Common.h"

typedef enum
{
    Nothing = 0,
    SimulateDaltonism,
    DaltonizeV1,
    SwitchCbCr,
    SwitchAndFlipCbCr,
    InvertLightness,
    HighlightColorUnderMouse,
    HighlightExactColorUnderMouse,
    GrabScreenArea,
    
    NumProcessingModes
} DLProcessingMode;

@interface DLCpuProcessor : NSObject

@property DLBlindnessType blindnessType;
@property DLProcessingMode processingMode;

- (void)transformSrgbaBuffer:(uint8_t*)sRgbaBuffer width:(int)width height:(int)height;
@end
