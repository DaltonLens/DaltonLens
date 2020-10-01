//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "ColorConversion.h"

namespace dl
{

    SRGBAToLMSConverter::SRGBAToLMSConverter ()
    {
        _linearRgbToLmsMatrix = ColMajorMatrix3f(17.8824,   43.5161,  4.11935,
                                                 3.45565,   27.1554,  3.86714,
                                                 0.0299566, 0.184309, 1.46709);
        
        // Eigen::Matrix3f::Map(_lmsToLinearRgbMatrix.v) = Eigen::Matrix3f::Map(_linearRgbToLmsMatrix.v).inverse();
        _lmsToLinearRgbMatrix = ColMajorMatrix3f( 0.08094446,   -0.1305044,    0.1167211,
                                                 -0.01024854,    0.05401933,  -0.1136147,
                                                 -0.0003652968, -0.004121615,  0.6935114);
        
    }
    
    void SRGBAToLMSConverter :: convertToLms (const ImageSRGBA& srgbaImage, ImageLMS& lmsImage)
    {
        // FIXME: in theory we'd need to convert to linearRGB first. But I don't expect
        // the difference to be huge.
        
        lmsImage.ensureAllocatedBufferForSize(srgbaImage.width(), srgbaImage.height());
        
        const auto& m = _linearRgbToLmsMatrix;
        
        for (int r = 0; r < srgbaImage.height(); ++r)
        {
            const auto* srgbaRow = srgbaImage.atRowPtr(r);
            auto* lmsRow = lmsImage.atRowPtr(r);
            for (int c = 0; c < srgbaImage.width(); ++c)
            {
                const auto& srgba = srgbaRow[c];
                auto& lms = lmsRow[c];
                lms.l = m.m00*srgba.r + m.m01*srgba.g + m.m02*srgba.b;
                lms.m = m.m10*srgba.r + m.m11*srgba.g + m.m12*srgba.b;
                lms.s = m.m20*srgba.r + m.m21*srgba.g + m.m22*srgba.b;
            }
        }
    }
    
    void SRGBAToLMSConverter :: convertToSrgba (const ImageLMS& lmsImage, ImageSRGBA& srgbaImage)
    {
        // FIXME: in theory we'd need to convert from linearRGB to SRGBBut I don't expect
        // the difference to be huge.
        
        srgbaImage.ensureAllocatedBufferForSize(lmsImage.width(), lmsImage.height());
        
        const auto& m = _lmsToLinearRgbMatrix;
                
        for (int r = 0; r < lmsImage.height(); ++r)
        {
            const auto* lmsRow = lmsImage.atRowPtr(r);
            auto* srgbaRow = srgbaImage.atRowPtr(r);
            for (int c = 0; c < lmsImage.width(); ++c)
            {
                const auto& lms = lmsRow[c];
                auto& srgba = srgbaRow[c];
                
                srgba.r = saturateAndCast(m.m00*lms.l + m.m01*lms.m + m.m02*lms.s);
                srgba.g = saturateAndCast(m.m10*lms.l + m.m11*lms.m + m.m12*lms.s);
                srgba.b = saturateAndCast(m.m20*lms.l + m.m21*lms.m + m.m22*lms.s);
                srgba.a = 255;
            }
        }
    }

} // dl

namespace dl
{
    
    // template to allow inlining of the lambda.
    template <typename CbCrTransform>
    void switchCbCr (ImageSRGBA& srgbaImage, CbCrTransform cbcrTransform)
    {
        /*
         from the python scripts.
         
         rgbToYCrgCb [[ 0.57735027  0.57735027  0.57735027]
                      [ 0.70710678 -0.70710678  0.        ]
                      [-0.40824829 -0.40824829  0.81649658]]
         */
        
        /*
         YCrgCbToRgb
         [[  5.77350269e-01   7.07106781e-01   4.08248290e-01]
          [  5.77350269e-01  -7.07106781e-01   4.08248290e-01]
          [  5.77350269e-01   9.06493304e-17  -8.16496581e-01]]
         */
        
        for (int r = 0; r < srgbaImage.height(); ++r)
        {
            auto* srgbaRow = srgbaImage.atRowPtr(r);
            for (int c = 0; c < srgbaImage.width(); ++c)
            {
                auto srgba = srgbaRow[c];
                auto& srgbaOut = srgbaRow[c];
                
                float y =   0.57735027*srgba.r + 0.57735027*srgba.g + 0.57735027*srgba.b;
                float cr =  0.70710678*srgba.r - 0.70710678*srgba.g;
                float cb = -0.40824829*srgba.r - 0.40824829*srgba.g + 0.81649658*srgba.b;
                
                cbcrTransform (y, cb, cr);
                
                srgbaOut.r = saturateAndCast(5.77350269e-01*y + 7.07106781e-01*cr - 4.08248290e-01*cb);
                srgbaOut.g = saturateAndCast(5.77350269e-01*y - 7.07106781e-01*cr - 4.08248290e-01*cb);
                srgbaOut.b = saturateAndCast(5.77350269e-01*y +            0.0*cr + 8.16496581e-01*cb);
            }
        }
    }
    
    void CbCrTransformer :: switchCbCr (ImageSRGBA& srgbaImage)
    {
        /*
         rgbToYCrgCb [[ 0.57735027  0.57735027  0.57735027]
         [ 0.70710678 -0.70710678  0.        ]
         [-0.40824829 -0.40824829  0.81649658]]
         */
        
        /*
         YCrgCbToRgb
         [[  5.77350269e-01   7.07106781e-01   4.08248290e-01]
         [  5.77350269e-01  -7.07106781e-01   4.08248290e-01]
         [  5.77350269e-01   9.06493304e-17  -8.16496581e-01]]
         */
        
        dl::switchCbCr (srgbaImage, [](float& y, float& cb, float& cr){
            
            std::swap (cb, cr);
            
        });
    }
    
    void CbCrTransformer :: switchAndFlipCbCr (ImageSRGBA& srgbaImage)
    {
        /*
         rgbToYCrgCb [[ 0.57735027  0.57735027  0.57735027]
         [ 0.70710678 -0.70710678  0.        ]
         [-0.40824829 -0.40824829  0.81649658]]
         */
        
        /*
         YCrgCbToRgb
         [[  5.77350269e-01   7.07106781e-01   4.08248290e-01]
         [  5.77350269e-01  -7.07106781e-01   4.08248290e-01]
         [  5.77350269e-01   9.06493304e-17  -8.16496581e-01]]
         */
        
        dl::switchCbCr (srgbaImage, [](float& y, float& cb, float& cr){
            
            std::swap (cb, cr);
            cb = -cb;
            
        });
    }

    PixelYCbCr convertToYCbCr(const PixelSRGBA& srgba)
    {
        float y =   0.57735027*srgba.r + 0.57735027*srgba.g + 0.57735027*srgba.b;
        float cr =  0.70710678*srgba.r - 0.70710678*srgba.g;
        float cb = -0.40824829*srgba.r - 0.40824829*srgba.g + 0.81649658*srgba.b;
        return PixelYCbCr(y, cb, cr);
    }
    
} // dl
