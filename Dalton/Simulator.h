//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include "Image.h"

#include "Common.h"

#include "ColorConversion.h"

#include "MathUtils.h"

namespace dl
{
    
    class Simulator
    {
    public:
        Simulator ();
        
    public:
        void applySimulation (ImageLMS& lmsImage, DLBlindnessType blindness);
        void daltonizeV1 (ImageSRGBA& srgbaImage, DLBlindnessType blindness);
        
    private:
        SRGBAToLMSConverter converter;
    };
    
} // dl
