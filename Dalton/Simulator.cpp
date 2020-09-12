//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Simulator.h"

namespace dl
{
    
    Simulator :: Simulator ()
    {
    }
    
    void Simulator :: applySimulation (ImageLMS& lmsImage, DLBlindnessType blindness)
    {
        // From daltonize.py:
        //        lms2lmsp = [0 2.02344 -2.52581; 0 1 0; 0 0 1] ;
        //        lms2lmsd = [1 0 0; 0.494207 0 1.24827; 0 0 1] ;
        //        lms2lmst = [1 0 0; 0 1 0; -0.395913 0.801109 0] ;
        
        auto protanope = [](int c, int r, PixelLMS& p) {
            p.l = /* 0*p.l + */ 2.02344*p.m - 2.52581*p.s;
        };
        
        auto deuteranope = [](int c, int r, PixelLMS& p) {
            p.m = 0.494207*p.l /* + 0*p.m */ + 1.24827*p.s;
        };
        
        auto tritanope = [](int c, int r, PixelLMS& p) {
            p.s = -0.395913*p.l + 0.801109*p.m /* + 0*p.s */;
        };
        
        switch (blindness)
        {
            case Protanope:
                lmsImage.apply (protanope);
                break;
                
            case Deuteranope:
                lmsImage.apply (deuteranope);
                break;
                
            case Tritanope:
                lmsImage.apply (tritanope);
                break;
                
            default:
                assert (false);
        }
    }
    
    void Simulator :: daltonizeV1 (ImageSRGBA& srgbaImage, DLBlindnessType blindness)
    {
        ImageLMS lmsImage;
        converter.convertToLms (srgbaImage, lmsImage);
        applySimulation (lmsImage, blindness);
        
        ImageSRGBA simulatedSRGBA;
        converter.convertToSrgba(lmsImage, simulatedSRGBA);
        
        srgbaImage.apply([&](int c, int r, PixelSRGBA& srgba) {

            // [0, 0, 0],
            // [0.7, 1, 0],
            // [0.7, 0, 1]
            
            const auto& simRgba = simulatedSRGBA(c,r);
            
            float rError = srgba.r - simRgba.r;
            float gError = srgba.g - simRgba.g;
            float bError = srgba.b - simRgba.b;
            
            float updatedG = srgba.g + 0.7*rError + 1.0*gError + 0.0*bError;
            float updatedB = srgba.b + 0.7*rError + 0.0*gError + 1.0*bError;
            srgba.g = saturateAndCast(updatedG);
            srgba.b = saturateAndCast(updatedB);
        });
        
    }
    
} // dl
