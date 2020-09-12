//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import "CpuProcessor.h"

#include "ColorConversion.h"
#include "Simulator.h"
#include "Image.h"
#include "Utils.h"

using namespace dl;

@interface DLCpuProcessor()
{
    SRGBAToLMSConverter converter;
    Simulator simulator;
    CbCrTransformer cbcrTransformer;
}
@end

@implementation DLCpuProcessor

- (void)transformSrgbaBuffer:(uint8_t*)sRgbaBuffer width:(int)width height:(int)height
{
    ScopeTimer sw ("transformSrgbaBuffer");
    
    ImageSRGBA srgbaImage (sRgbaBuffer,
                           width,
                           height,
                           width*4,
                           ImageSRGBA::noopReleaseFunc());
    
    dl_dbg("Blindness type: %d", (int)self.blindnessType);
    dl_dbg("Processing mode: %d", (int)self.processingMode);
    
    switch (self.processingMode)
    {
        case Nothing:
        {
            break;
        }
            
        case SimulateDaltonism:
        {
            ImageLMS lmsImage;
            converter.convertToLms(srgbaImage, lmsImage);
            simulator.applySimulation(lmsImage, self.blindnessType);
            converter.convertToSrgba(lmsImage, srgbaImage);
            break;
        }
            
        case DaltonizeV1:
        {
            simulator.daltonizeV1(srgbaImage, self.blindnessType);
            break;
        }
            
        case SwitchCbCr:
        {
            cbcrTransformer.switchCbCr(srgbaImage);
            break;
        }
            
        case SwitchAndFlipCbCr:
        {
            cbcrTransformer.switchAndFlipCbCr(srgbaImage);
            break;
        }
            
        default:
        {
            // not implemented in CPU
            assert (false);
            break;
        }
    }
    
}

@end
