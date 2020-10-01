//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include "Image.h"
#include "MathUtils.h"

namespace dl
{
    
    class SRGBAToLMSConverter
    {
    public:
        SRGBAToLMSConverter ();
        
        void convertToLms (const ImageSRGBA& srgbaImage, ImageLMS& lmsImage);
        void convertToSrgba (const ImageLMS& lmsImage, ImageSRGBA& srgbaImage);
        
    private:
        ColMajorMatrix3f _linearRgbToLmsMatrix;
        ColMajorMatrix3f _lmsToLinearRgbMatrix;
    };
    
    class CbCrTransformer
    {
    public:
        void switchCbCr (ImageSRGBA& srgbaImage);
        void switchAndFlipCbCr (ImageSRGBA& srgbaImage);
    };

    PixelYCbCr convertToYCbCr(const PixelSRGBA& p);
    
} // dl
