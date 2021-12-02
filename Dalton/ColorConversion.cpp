//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "ColorConversion.h"

#include <cmath>

namespace dl
{

    RGBAToLMSConverter::RGBAToLMSConverter ()
    {
        // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.LMS_from_linearRGB
        // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.linearRGB_from_LMS

        _linearRgbToLmsMatrix = ColMajorMatrix3f(0.17886, 0.43997, 0.03597,
                                                 0.0338 , 0.27515, 0.03621,
                                                 0.00031, 0.00192, 0.01528);
        
        // Eigen::Matrix3f::Map(_lmsToLinearRgbMatrix.v) = Eigen::Matrix3f::Map(_linearRgbToLmsMatrix.v).inverse();
        _lmsToLinearRgbMatrix = ColMajorMatrix3f( 8.00533, -12.88195,  11.68065,
                                                 -0.97821, 5.26945, -10.183,
                                                 -0.04017, -0.39885, 66.48079);
    }
    
    void RGBAToLMSConverter :: convertToLms (const ImageLinearRGB& rgbImage, ImageLMS& lmsImage)
    {        
        lmsImage.ensureAllocatedBufferForSize(rgbImage.width(), rgbImage.height());
        
        const auto& m = _linearRgbToLmsMatrix;
        
        for (int r = 0; r < rgbImage.height(); ++r)
        {
            const auto* rgbRow = rgbImage.atRowPtr(r);
            auto* lmsRow = lmsImage.atRowPtr(r);
            for (int c = 0; c < rgbImage.width(); ++c)
            {
                const auto& srgba = rgbRow[c];
                auto& lms = lmsRow[c];
                lms.l = m.m00*srgba.r + m.m01*srgba.g + m.m02*srgba.b;
                lms.m = m.m10*srgba.r + m.m11*srgba.g + m.m12*srgba.b;
                lms.s = m.m20*srgba.r + m.m21*srgba.g + m.m22*srgba.b;
            }
        }
    }
    
    void RGBAToLMSConverter :: convertToLinearRGB (const ImageLMS& lmsImage, ImageLinearRGB& rgbImage)
    {
        rgbImage.ensureAllocatedBufferForSize(lmsImage.width(), lmsImage.height());
        
        const auto& m = _lmsToLinearRgbMatrix;
                
        for (int r = 0; r < lmsImage.height(); ++r)
        {
            const auto* lmsRow = lmsImage.atRowPtr(r);
            auto* rgbRow = rgbImage.atRowPtr(r);
            for (int c = 0; c < lmsImage.width(); ++c)
            {
                const auto& lms = lmsRow[c];
                auto& rgb = rgbRow[c];
                
                rgb.r = m.m00*lms.l + m.m01*lms.m + m.m02*lms.s;
                rgb.g = m.m10*lms.l + m.m11*lms.m + m.m12*lms.s;
                rgb.b = m.m20*lms.l + m.m21*lms.m + m.m22*lms.s;
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

    // From https://www.nayuki.io/res/srgb-transform-library
    double srgbToLinear (double x)
    {
        if (x <= 0.0)
            return 0.0;
        else if (x >= 1.0)
            return 1.0;
        else if (x < 0.04045)
            return x / 12.92;
        else
            return std::pow((x + 0.055) / 1.055, 2.4);
    }

    // From https://www.nayuki.io/res/srgb-transform-library
    double linearToSrgb(double x)
    {
        if (x <= 0.0)
            return 0.0;
        else if (x >= 1.0)
            return 1.0;
        else if (x < 0.0031308)
            return x * 12.92;
        else
            return std::pow(x, 1.0 / 2.4) * 1.055 - 0.055;
    }

    PixelLinearRGB convertToLinearRGB(const PixelSRGBA& srgb)
    {
        return PixelLinearRGB(srgbToLinear(srgb.r/255.0), srgbToLinear(srgb.g/255.0), srgbToLinear(srgb.b/255.0));
    }

    PixelSRGBA convertToSRGBA(const PixelLinearRGB& rgb)
    {
        return PixelSRGBA(roundAndSaturateToUint8(linearToSrgb(rgb.r)*255.0),
                          roundAndSaturateToUint8(linearToSrgb(rgb.g)*255.0),
                          roundAndSaturateToUint8(linearToSrgb(rgb.b)*255.0),
                          255);
    }

    ImageSRGBA convertToSRGBA(const ImageLinearRGB& rgb)
    {
        const int w = rgb.width();
        const int h = rgb.height();
        ImageSRGBA outImg(w, h);
        for (int r = 0; r < h; ++r)
        {
            const auto* inPtr = rgb.atRowPtr(r);
            const auto* lastInPtr = rgb.atRowPtr(r+1);
            auto* outPtr = outImg.atRowPtr(r);
            while (inPtr != lastInPtr)
            {
                *outPtr = convertToSRGBA(*inPtr);
                ++inPtr;
                ++outPtr;
            }
        }
        return outImg;
    }

    ImageLinearRGB convertToLinearRGB(const ImageSRGBA& srgb)
    {
        const int w = srgb.width();
        const int h = srgb.height();
        ImageLinearRGB outImg(w, h);
        for (int r = 0; r < h; ++r)
        {
            const auto* inPtr = srgb.atRowPtr(r);
            const auto* lastInPtr = srgb.atRowPtr(r+1);
            auto* outPtr = outImg.atRowPtr(r);
            while (inPtr != lastInPtr)
            {
                *outPtr = convertToLinearRGB(*inPtr);
                ++inPtr;
                ++outPtr;
            }
        }
        return outImg;
    }

    // Convert rgb floats ([0-255],[0-255],[0-255])
    // to hsv floats ([0-1],[0-1],[0-255]), from Foley & van Dam p592
    // Optimized http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
    // Adapted from ImGui ColorConvertRGBtoHSV
    PixelHSV convertToHSV(const PixelSRGBA& srgba)
    {
        // p in range [0,1]
        // PixelLinearRGB p = convertToLinearRGB(srgba);
        // HSV itself is defined for any RGB color space, but it is
        // typically applied on top of the raw sRGB values, not linearRGB.
        float r = srgba.r;
        float g = srgba.g;
        float b = srgba.b;
        
        float K = 0.f;
        if (g < b)
        {
            std::swap(g, b);
            K = -1.f;
        }
        if (r < g)
        {
            std::swap(r, g);
            K = -2.f / 6.f - K;
        }
        
        const float chroma = r - (g < b ? g : b);
        
        PixelHSV hsv;
        hsv.x = std::abs(K + (g - b) / (6.f * chroma + 1e-20f));
        hsv.y = chroma / (r + 1e-20f);
        hsv.z = r;
        return hsv;
    }

    // Convert hsv floats ([0-1],[0-1],[0-255]) to rgb floats ([0-255],[0-255],[0-255]), from Foley & van Dam p593
    // also http://en.wikipedia.org/wiki/HSL_and_HSV
    // Adapted from ImGui ColorConvertHSVtoRGB.
    PixelSRGBA convertToSRGBA(const PixelHSV& hsv)
    {
        float h = hsv.x;
        const float s = hsv.y;
        const float v_float = hsv.z;
        
        // Note: int(float_value+0.5f) is the same as a round + convert to int.
        
        if (s < 1e-10f)
        {
            // gray
            const uint8_t v = roundAndSaturateToUint8(v_float);
            return PixelSRGBA(v,v,v,255);
        }
        
        h = fmodf(h, 1.0f) / (60.0f / 360.0f);
        int   i = (int)h;
        float f = h - (float)i;
        float p_float = v_float * (1.0f - s);
        float q_float = v_float * (1.0f - s * f);
        float t_float = v_float * (1.0f - s * (1.0f - f));
        
        uint8_t v = roundAndSaturateToUint8(v_float);
        uint8_t p = roundAndSaturateToUint8(p_float);
        uint8_t q = roundAndSaturateToUint8(q_float);
        uint8_t t = roundAndSaturateToUint8(t_float);
        
        switch (i)
        {
            case 0: return PixelSRGBA(v, t, p, 255); break;
            case 1: return PixelSRGBA(q, v, p, 255); break;
            case 2: return PixelSRGBA(p, v, t, 255); break;
            case 3: return PixelSRGBA(p, q, v, 255); break;
            case 4: return PixelSRGBA(t, p, v, 255); break;
            case 5: default: return PixelSRGBA(v, p, q, 255); break;
        }
    }

    PixelXYZ convertToXYZ(const PixelSRGBA& srgb)
    {
        // PixelLinearRGB rgb = convertToLinearRGB(srgb);
        double r = srgb.r/255.0;
        double g = srgb.g/255.0;
        double b = srgb.b/255.0;

        r = ((r > 0.04045) ? pow((r + 0.055) / 1.055, 2.4) : (r / 12.92)) * 100.0;
        g = ((g > 0.04045) ? pow((g + 0.055) / 1.055, 2.4) : (g / 12.92)) * 100.0;
        b = ((b > 0.04045) ? pow((b + 0.055) / 1.055, 2.4) : (b / 12.92)) * 100.0;

        PixelXYZ xyz;
        xyz.x = r*0.4124564 + g*0.3575761 + b*0.1804375;
        xyz.y = r*0.2126729 + g*0.7151522 + b*0.0721750;
        xyz.z = r*0.0193339 + g*0.1191920 + b*0.9503041;
        return xyz;
    }

    PixelSRGBA convertToSRGBA(const PixelXYZ& xyz)
    {
        PixelSRGBA srgb;
        double x = xyz.x / 100.0;
        double y = xyz.y / 100.0;
        double z = xyz.z / 100.0;

        double r = x * 3.2404542 + y * -1.5371385 + z * -0.4985314;
        double g = x * -0.9692660 + y * 1.8760108 + z * 0.0415560;
        double b = x * 0.0556434 + y * -0.2040259 + z * 1.0572252;

        r = ((r > 0.0031308) ? (1.055*pow(r, 1 / 2.4) - 0.055) : (12.92*r));
        g = ((g > 0.0031308) ? (1.055*pow(g, 1 / 2.4) - 0.055) : (12.92*g));
        b = ((b > 0.0031308) ? (1.055*pow(b, 1 / 2.4) - 0.055) : (12.92*b));
        
        srgb.r = roundAndSaturateToUint8(r*255.0);
        srgb.g = roundAndSaturateToUint8(g*255.0);
        srgb.b = roundAndSaturateToUint8(b*255.0);
        srgb.a = 255;
        
        return srgb;
    }

    PixelLab convertToLab(const PixelSRGBA& srgb)
    {
        PixelXYZ xyz = convertToXYZ(srgb);
        
        double x = xyz.x / 95.047;
        double y = xyz.y / 100.00;
        double z = xyz.z / 108.883;
        
        x = (x > 0.008856) ? cbrt(x) : (7.787 * x + 16.0 / 116.0);
        y = (y > 0.008856) ? cbrt(y) : (7.787 * y + 16.0 / 116.0);
        z = (z > 0.008856) ? cbrt(z) : (7.787 * z + 16.0 / 116.0);
    
        PixelLab lab;
        lab.l = (116.0 * y) - 16.;
        lab.a = 500. * (x - y);
        lab.b = 200. * (y - z);
        return lab;
    }

    PixelSRGBA convertToSRGBA(const PixelLab& lab)
    {
        PixelXYZ xyz;
        xyz.y = (lab.l + 16.0) / 116.0;
        xyz.x = lab.a / 500.0 + xyz.y;
        xyz.z = xyz.y - lab.b / 200.0;

        #define POW3(x) ((x)*(x)*(x))
        double x3 = POW3(xyz.x);
        double y3 = POW3(xyz.y);
        double z3 = POW3(xyz.z);
        #undef POW3

        xyz.x = ((x3 > 0.008856) ? x3 : ((xyz.x - 16.0 / 116.0) / 7.787)) * 95.047;
        xyz.y = ((y3 > 0.008856) ? y3 : ((xyz.y - 16.0 / 116.0) / 7.787)) * 100.0;
        xyz.z = ((z3 > 0.008856) ? z3 : ((xyz.z - 16.0 / 116.0) / 7.787)) * 108.883;
        
        return convertToSRGBA(xyz);
    }
    
} // dl

namespace dl
{
    double colorDistance_RGBL1(const PixelSRGBA& p1, const PixelSRGBA& p2)
    {
        return (std::abs(p1.r - p2.r) + std::abs(p1.g - p2.g) + std::abs(p1.b - p2.b)) / 3.0;
    }

    // github.com/berendeanicolae/ColorSpace
    double colorDistance_CIE2000(const PixelLab& lab_1, const PixelLab& lab_2)
    {
        const double eps = 1e-5;
            
        // calculate ci, hi, i=1,2
        double c1 = sqrt(sqr(lab_1.a) + sqr(lab_1.b));
        double c2 = sqrt(sqr(lab_2.a) + sqr(lab_2.b));
        double meanC = (c1 + c2) / 2.0;
        double meanC7 = pow7(meanC);

        double g = 0.5*(1 - sqrt(meanC7 / (meanC7 + 6103515625.))); // 0.5*(1-sqrt(meanC^7/(meanC^7+25^7)))
        double a1p = lab_1.a * (1 + g);
        double a2p = lab_2.a * (1 + g);

        c1 = sqrt(sqr(a1p) + sqr(lab_1.b));
        c2 = sqrt(sqr(a2p) + sqr(lab_2.b));
        double h1 = fmod(atan2(lab_1.b, a1p) + 2*M_PI, 2*M_PI);
        double h2 = fmod(atan2(lab_2.b, a2p) + 2*M_PI, 2*M_PI);

        // compute deltaL, deltaC, deltaH
        double deltaL = lab_2.l - lab_1.l;
        double deltaC = c2 - c1;
        double deltah;

        if (c1*c2 < eps) {
            deltah = 0;
        }
        if (std::abs(h2 - h1) <= M_PI) {
            deltah = h2 - h1;
        }
        else if (h2 > h1) {
            deltah = h2 - h1 - 2* M_PI;
        }
        else {
            deltah = h2 - h1 + 2 * M_PI;
        }

        double deltaH = 2 * sqrt(c1*c2)*sin(deltah / 2);

        // calculate CIEDE2000
        double meanL = (lab_1.l + lab_2.l) / 2;
        meanC = (c1 + c2) / 2.0;
        meanC7 = pow7(meanC);
        double meanH;

        if (c1*c2 < eps) {
            meanH = h1 + h2;
        }
        if (std::abs(h1 - h2) <= M_PI + eps) {
            meanH = (h1 + h2) / 2;
        }
        else if (h1 + h2 < 2*M_PI) {
            meanH = (h1 + h2 + 2*M_PI) / 2;
        }
        else {
            meanH = (h1 + h2 - 2*M_PI) / 2;
        }

        double T = 1
            - 0.17*cos(meanH - Deg2Rad*30)
            + 0.24*cos(2 * meanH)
            + 0.32*cos(3 * meanH + Deg2Rad*6)
            - 0.2*cos(4 * meanH - Deg2Rad*63);
        double sl = 1 + (0.015*sqr(meanL - 50)) / sqrt(20 + sqr(meanL - 50));
        double sc = 1 + 0.045*meanC;
        double sh = 1 + 0.015*meanC*T;
        double rc = 2 * sqrt(meanC7 / (meanC7 + 6103515625.));
        double rt = -sin(Deg2Rad*(60.0 * exp(-sqr((Rad2Deg*meanH - 275.0) / 25.0)))) * rc;

        return sqrt(sqr(deltaL / sl) + sqr(deltaC / sc) + sqr(deltaH / sh) + rt * deltaC / sc * deltaH / sh);
    }

} // dl

namespace dl
{

// This comes from the extended colors https://en.wikipedia.org/wiki/Web_colors
// The extended colors is the result of merging specifications from HTML 4.01, CSS 2.0, SVG 1.0 and CSS3 User Interfaces (CSS3 UI)
// https://en.wikipedia.org/wiki/Template:Color_chart_X11
static ColorEntry colorEntries[] = {
    {"Pink","Pink",255,192,203},
    {"Pink","LightPink",255,182,193},
    {"Pink","HotPink",255,105,180},
    {"Pink","DeepPink",255,20,147},
    {"Pink","PaleVioletRed",219,112,147},
    {"Pink","MediumVioletRed",199,21,133},
    {"Red","LightSalmon",255,160,122},
    {"Red","Salmon",250,128,114},
    {"Red","DarkSalmon",233,150,122},
    {"Red","LightCoral",240,128,128},
    {"Red","IndianRed",205,92,92},
    {"Red","Crimson",220,20,60},
    {"Red","Firebrick",178,34,34},
    {"Red","DarkRed",139,0,0},
    {"Red","Red",255,0,0},
    {"Orange","OrangeRed",255,69,0},
    {"Orange","Tomato",255,99,71},
    {"Orange","Coral",255,127,80},
    {"Orange","DarkOrange",255,140,0},
    {"Orange","Orange",255,165,0},
    {"Yellow","Yellow",255,255,0},
    {"Yellow","LightYellow",255,255,224},
    {"Yellow","LemonChiffon",255,250,205},
    {"Yellow","LightGoldenrodYellow",250,250,210},
    {"Yellow","PapayaWhip",255,239,213},
    {"Yellow","Moccasin",255,228,181},
    {"Yellow","PeachPuff",255,218,185},
    {"Yellow","PaleGoldenrod",238,232,170},
    {"Yellow","Khaki",240,230,140},
    {"Yellow","DarkKhaki",189,183,107},
    {"Yellow","Gold",255,215,0},
    {"Brown","Cornsilk",255,248,220},
    {"Brown","BlanchedAlmond",255,235,205},
    {"Brown","Bisque",255,228,196},
    {"Brown","NavajoWhite",255,222,173},
    {"Brown","Wheat",245,222,179},
    {"Brown","Burlywood",222,184,135},
    {"Brown","Tan",210,180,140},
    {"Brown","RosyBrown",188,143,143},
    {"Brown","SandyBrown",244,164,96},
    {"Brown","Goldenrod",218,165,32},
    {"Brown","DarkGoldenrod",184,134,11},
    {"Brown","Peru",205,133,63},
    {"Brown","Chocolate",210,105,30},
    {"Brown","SaddleBrown",139,69,19},
    {"Brown","Sienna",160,82,45},
    {"Brown","Brown",165,42,42},
    {"Brown","Maroon",128,0,0},
    {"Green","DarkOliveGreen",85,107,47},
    {"Green","Olive",128,128,0},
    {"Green","OliveDrab",107,142,35},
    {"Green","YellowGreen",154,205,50},
    {"Green","LimeGreen",50,205,50},
    {"Green","Lime",0,255,0},
    {"Green","LawnGreen",124,252,0},
    {"Green","Chartreuse",127,255,0},
    {"Green","GreenYellow",173,255,47},
    {"Green","SpringGreen",0,255,127},
    {"Green","MediumSpringGreen",0,250,154},
    {"Green","LightGreen",144,238,144},
    {"Green","PaleGreen",152,251,152},
    {"Green","DarkSeaGreen",143,188,143},
    {"Green","MediumAquamarine",102,205,170},
    {"Green","MediumSeaGreen",60,179,113},
    {"Green","SeaGreen",46,139,87},
    {"Green","ForestGreen",34,139,34},
    {"Green","Green",0,128,0},
    {"Green","DarkGreen",0,100,0},
    {"Cyan","Aqua",0,255,255},
    {"Cyan","Cyan",0,255,255},
    {"Cyan","LightCyan",224,255,255},
    {"Cyan","PaleTurquoise",175,238,238},
    {"Cyan","Aquamarine",127,255,212},
    {"Cyan","Turquoise",64,224,208},
    {"Cyan","MediumTurquoise",72,209,204},
    {"Cyan","DarkTurquoise",0,206,209},
    {"Cyan","LightSeaGreen",32,178,170},
    {"Cyan","CadetBlue",95,158,160},
    {"Cyan","DarkCyan",0,139,139},
    {"Cyan","Teal",0,128,128},
    {"Blue","LightSteelBlue",176,196,222},
    {"Blue","PowderBlue",176,224,230},
    {"Blue","LightBlue",173,216,230},
    {"Blue","SkyBlue",135,206,235},
    {"Blue","LightSkyBlue",135,206,250},
    {"Blue","DeepSkyBlue",0,191,255},
    {"Blue","DodgerBlue",30,144,255},
    {"Blue","CornflowerBlue",100,149,237},
    {"Blue","SteelBlue",70,130,180},
    {"Blue","RoyalBlue",65,105,225},
    {"Blue","Blue",0,0,255},
    {"Blue","MediumBlue",0,0,205},
    {"Blue","DarkBlue",0,0,139},
    {"Blue","Navy",0,0,128},
    {"Blue","MidnightBlue",25,25,112},
    {"Violet","Lavender",230,230,250},
    {"Violet","Thistle",216,191,216},
    {"Violet","Plum",221,160,221},
    {"Violet","Violet",238,130,238},
    {"Violet","Orchid",218,112,214},
    // Remove the duplicate color.
    // {"Violet","Fuchsia",255,0,255},
    {"Violet","Magenta",255,0,255},
    {"Violet","MediumOrchid",186,85,211},
    {"Violet","MediumPurple",147,112,219},
    {"Violet","BlueViolet",138,43,226},
    {"Violet","DarkViolet",148,0,211},
    {"Violet","DarkOrchid",153,50,204},
    {"Violet","DarkMagenta",139,0,139},
    {"Violet","Purple",128,0,128},
    {"Violet","Indigo",75,0,130},
    {"Violet","DarkSlateBlue",72,61,139},
    {"Violet","SlateBlue",106,90,205},
    {"Violet","MediumSlateBlue",123,104,238},
    {"White","White",255,255,255},
    {"White","Snow",255,250,250},
    {"White","Honeydew",240,255,240},
    {"White","MintCream",245,255,250},
    {"White","Azure",240,255,255},
    {"White","AliceBlue",240,248,255},
    {"White","GhostWhite",248,248,255},
    {"White","WhiteSmoke",245,245,245},
    {"White","Seashell",255,245,238},
    {"White","Beige",245,245,220},
    {"White","OldLace",253,245,230},
    {"White","FloralWhite",255,250,240},
    {"White","Ivory",255,255,240},
    {"White","AntiqueWhite",250,235,215},
    {"White","Linen",250,240,230},
    {"White","LavenderBlush",255,240,245},
    {"White","MistyRose",255,228,225},
    {"Gray","Gainsboro",220,220,220},
    {"Gray","LightGray",211,211,211},
    {"Gray","Silver",192,192,192},
    {"Gray","DarkGray",169,169,169},
    {"Gray","Gray",128,128,128},
    {"Gray","DimGray",105,105,105},
    {"Gray","LightSlateGray",119,136,153},
    {"Gray","SlateGray",112,128,144},
    {"Gray","DarkSlateGray",47,79,79},
    {"Gray","Black",0,0,0}
};

void closestColorName (const PixelSRGBA& rgba, const char** className, const char** colorName)
{
    const int numColors = sizeof(colorEntries) / sizeof(ColorEntry);
    int bestIndex = 0;
    uint32_t bestDist = std::numeric_limits<uint32_t>::max();
    for (int i = 0; i < numColors; ++i)
    {
        const auto& colorName = colorEntries[i];
        uint32_t dist = std::abs(rgba.r - colorName.r) + std::abs(rgba.g - colorName.g) + std::abs(rgba.b - colorName.b);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestIndex = i;
        }
    }
    
    *className = colorEntries[bestIndex].className;
    *colorName = colorEntries[bestIndex].colorName;
}

std::array<ColorMatchingResult,2> closestColorEntries (const PixelSRGBA& srgba, ColorDistance distance)
{
    std::array<ColorMatchingResult,2> closestColors;
    closestColors[0].distance = std::numeric_limits<double>::max();
    closestColors[1].distance = std::numeric_limits<double>::max();
    
    const int numColors = sizeof(colorEntries) / sizeof(ColorEntry);
    PixelLab targetLab = convertToLab(srgba);
    for (int i = 0; i < numColors; ++i)
    {
        const auto& colorEntry = colorEntries[i];
        double dist;
        switch (distance)
        {
            case ColorDistance::RGB_L1: dist = colorDistance_RGBL1(srgba, PixelSRGBA(colorEntry.r, colorEntry.g, colorEntry.b, 255)); break;
            case ColorDistance::CIE2000: dist = colorDistance_CIE2000(targetLab,
                                                                      convertToLab(PixelSRGBA(colorEntry.r, colorEntry.g, colorEntry.b, 255))); break;
        }
        
        if (dist < closestColors[0].distance)
        {
            closestColors[1] = closestColors[0];
            closestColors[0].distance = dist;
            closestColors[0].indexInTable = i;
        }
        else if (dist < closestColors[1].distance)
        {
            closestColors[1].distance = dist;
            closestColors[1].indexInTable = i;
        }
    }
    
    closestColors[0].entry = &colorEntries[closestColors[0].indexInTable];
    closestColors[1].entry = &colorEntries[closestColors[1].indexInTable];
    
    return closestColors;
}

} // dl
