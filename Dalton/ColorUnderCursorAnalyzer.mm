//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import "ColorUnderCursorAnalyzer.h"

#include "Utils.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {

    inline float colorfullNess (const DLRGBAPixel& p)
    {
        float drg = std::abs(p.r-p.g);
        float drb = std::abs(p.r-p.b);
        float dgb = std::abs(p.g-p.b);
        return drg + drb + dgb;
    }
    
    inline DLRGBAPixel sRgbToLinear (const DLRGBAPixel& srgba)
    {
        auto sRgbToLinearU8 = [](uint8_t s) -> float {
            const float S = s/255.f;
            if (S <= 0.04045f) return S/12.92f;
            return 255.f*(powf((S+0.055f)/1.055f, 2.4f));
        };
        
        DLRGBAPixel lrgba;
        lrgba.r = sRgbToLinearU8(srgba.r);
        lrgba.g = sRgbToLinearU8(srgba.g);
        lrgba.b = sRgbToLinearU8(srgba.b);
        lrgba.a = srgba.a;
        return lrgba;
    }
    
} // anonymous

@implementation DLColorUnderCursorAnalyzer

-(DLRGBAPixel)dominantColorInBuffer:(const uint8_t*)data
                             width:(size_t)width
                            height:(size_t)height
                       bytesPerRow:(size_t)bytesPerRow
{
    const size_t bytesPerPixel = 4;
    auto pixelAt = [&](size_t c, size_t r) -> const DLRGBAPixel& {
        const auto& dataPtr = data[r*bytesPerRow + c*bytesPerPixel];
        return reinterpret_cast<const DLRGBAPixel&>(dataPtr);
    };
    
    DLRGBAPixel bestPixel = pixelAt(width/2, height/2);
    float maxColorfullNess = colorfullNess(bestPixel);
    
    if (maxColorfullNess < 10)
    {
        for (int r = 0; r < height; ++r)
            for (int c = 0; c < width; ++c)
            {
                const auto& p = pixelAt(c,r);
                auto pCF = colorfullNess(p);
                if (pCF > maxColorfullNess)
                {
                    maxColorfullNess = pCF;
                    bestPixel = p;
                }
            }
    }
    

    // DLRGBAPixel bestLinearPixel = sRgbToLinear(bestPixel);
    // dl_dbg("bestPixel SRGBA: %d %d %d %d", bestPixel.r, bestPixel.g, bestPixel.b, bestPixel.a);
    // dl_dbg("bestPixel LRGBA: %d %d %d %d",
    //         bestLinearPixel.r,
    //         bestLinearPixel.g,
    //         bestLinearPixel.b,
    //         bestLinearPixel.a);
    
    return bestPixel;
}

-(DLRGBAPixel)darkestColorInBuffer:(const uint8_t*)data
                           width:(size_t)width
                          height:(size_t)height
                     bytesPerRow:(size_t)bytesPerRow
{
    const size_t bytesPerPixel = 4;
    auto pixelAt = [&](size_t c, size_t r) -> const DLRGBAPixel& {
        const auto& dataPtr = data[r*bytesPerRow + c*bytesPerPixel];
        return reinterpret_cast<const DLRGBAPixel&>(dataPtr);
    };
    
    auto pixelMin = [](DLRGBAPixel p) {
        return std::min(p.r, std::min(p.g, p.b));
    };
    
    auto hasColor = [](DLRGBAPixel p) {
        return colorfullNess(p) > 10;
    };
    
    DLRGBAPixel bestPixel = pixelAt(width/2, height/2);
    float darkestMean = FLT_MAX;
    
    for (int r = 0; r < height; ++r)
    for (int c = 0; c < width; ++c)
    {
        const auto& p = pixelAt(c,r);
        if (!hasColor(p))
            continue;
        
        float mean = pixelMin(p);
        if (mean < darkestMean)
        {
            darkestMean = mean;
            bestPixel = p;
        }
    }
    
    // dl_dbg("bestPixel SRGBA: %d %d %d %d", bestPixel.r, bestPixel.g, bestPixel.b, bestPixel.a);
    
    // DLRGBAPixel bestLinearPixel = sRgbToLinear(bestPixel);
    //    dl_dbg("bestPixel LRGBA: %d %d %d %d",
    //           bestLinearPixel.r,
    //           bestLinearPixel.g,
    //           bestLinearPixel.b,
    //           bestLinearPixel.a);
    
    return bestPixel;
}
@end
