//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "OpenGL_Shaders.h"

namespace dl
{

const char* commonFragmentLibrary = R"(
    const float hsxEpsilon = 1e-10;

    // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.linearRGB_from_LMS

    // Derivation of the constant values here can be found in https://github.com/DaltonLens/libDaltonLens
    // and https://github.com/DaltonLens/DaltonLens-Python .

    // Warning: GLSL mat3 initialization is column by column
    // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.LMS_from_linearRGB
    const mat3 LMS_from_linearRGB = mat3 (
        0.17882, 0.03456, 0.00030, // column1
        0.43516, 0.27155, 0.00184, // column2
        0.04119, 0.03867, 0.01467  // column3      
    );

    const mat3 linearRGB_from_LMS = mat3 (
        8.09444, -1.02485, -0.03653,   // column1
        -13.05043, 5.40193, -0.41216,  // column2
        11.67206, -11.36147, 69.35132  // column3
    );

    // https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    // Converts a color from linear light gamma to sRGB gamma
    vec4 sRGB_from_RGB(vec4 linearRGB)
    {
        bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
        vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
        vec3 lower = linearRGB.rgb * vec3(12.92);

        return vec4(mix(higher, lower, cutoff), linearRGB.a);
    }

    vec4 sRGB_from_RGBClamped (vec4 rgba)
    {
        return sRGB_from_RGB(clamp(rgba, 0.0, 1.0));
    }

    // Converts a color from sRGB gamma to linear light gamma
    vec4 RGB_from_SRGB(vec4 sRGB)
    {
        bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
        vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));
        vec3 lower = sRGB.rgb/vec3(12.92);

        return vec4(mix(higher, lower, cutoff), sRGB.a);
    }

    vec3 YCbCr_from_RGBA (vec4 rgba)
    {
        float y =   0.57735027*rgba.r + 0.57735027*rgba.g + 0.57735027*rgba.b;
        float cr =  0.70710678*rgba.r - 0.70710678*rgba.g;
        float cb = -0.40824829*rgba.r - 0.40824829*rgba.g + 0.81649658*rgba.b;
        return vec3(y,cb,cr);
    }

    vec4 RGBA_from_YCbCr (vec3 yCbCr, float alpha)
    {
        vec4 rgbaOut;
        
        float y = yCbCr.x;
        float cb = yCbCr.y;
        float cr = yCbCr.z;
        
        rgbaOut.r = clamp(5.77350269e-01*y + 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
        rgbaOut.g = clamp(5.77350269e-01*y - 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
        rgbaOut.b = clamp(5.77350269e-01*y +            0.0*cr + 8.16496581e-01*cb, 0.0, 1.0);
        rgbaOut.a = alpha;
        return rgbaOut;
    }

    vec3 LMS_from_RGBA (vec4 rgba)
    {
        return LMS_from_linearRGB * rgba.xyz;
    }

    vec4 RGBA_from_LMS (vec3 lms, float alpha)
    {
        return vec4(linearRGB_from_LMS * lms.xyz, alpha);
    }

    // H in [0,1]
    // C in [0,1]
    // V in [0,1]
    vec3 HCV_from_SRGB(vec3 RGB)
    {
        // Based on work by Sam Hocevar and Emil Persson
        vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0/3.0) : vec4(RGB.gb, 0.0, -1.0/3.0);
        vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
        float C = Q.x - min(Q.w, Q.y);
        float H = abs((Q.w - Q.y) / (6 * C + hsxEpsilon) + Q.z);
        return vec3(H, C, Q.x);
    }
    
    // H in [0,1]
    // C in [0,1]
    // V in [0,1]
    vec3 HSV_from_SRGB (vec3 rgb)
    {
        // This ignores gamma, but that's how HSV
        // is typically done.
    
        vec3 HCV = HCV_from_SRGB(rgb.xyz);
        float S = HCV.y / (HCV.z + hsxEpsilon);
        return vec3(HCV.x, S, HCV.z);
    }

    vec3 RGB_from_HUE(float H)
    {
        float R = abs(H * 6 - 3) - 1;
        float G = 2 - abs(H * 6 - 2);
        float B = 2 - abs(H * 6 - 4);
        return clamp(vec3(R,G,B), 0, 1);
    }

    vec4 RGBA_from_HSV(vec3 HSV, float alpha)
    {
        vec3 RGB = RGB_from_HUE(HSV.x);
        return vec4(((RGB - 1) * HSV.y + 1) * HSV.z, alpha);
    }

    vec3 applyProtanope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        lms[0] = 2.02344*lms[1] - 2.52580*lms[2];
        return lms;
    }

    vec3 applyDeuteranope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        lms[1] = 0.49421*lms[0] + 1.24827*lms[2];
        return lms;
    }

    vec3 applyTritanope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        // WARNING: Viénot is not good for tritanopia, we need
        // to switch to Brettel.
        lms[2] = -0.01224*lms[0] + 0.07203*lms[1];
        return lms;
    }

    vec3 applyTritanope_Brettel1997 (vec3 lms)
    {
        // See libDaltonLens for the values.
        vec3 normalOfSepPlane = vec3(0.34478, -0.65518, 0.00000);
        if (dot(lms, normalOfSepPlane) >= 0)
        {
            // Plane 1 for tritanopia
            lms.z = -0.00257*lms.x + 0.05366*lms.y;
        }
        else
        {
            // Plane 2 for tritanopia
            lms.z = -0.06011*lms.x + 0.16299*lms.y;
        }
        return lms;
    }

    vec4 daltonizeV1 (vec4 rgba, vec4 rgbaSimulated)
    {
        vec3 error = rgba.rgb - rgbaSimulated.rgb;

        // FIXME: here all the errors is distributed from the red channel to the
        // green and blue one. This is a good idea for protanopes, but not for
        // the others. We need to adjust the m matrix for each deficiency type.
        float updatedG = rgba.g + 0.7*error.r + 1.0*error.g + 0.0*error.b;
        float updatedB = rgba.b + 0.7*error.r + 0.0*error.g + 1.0*error.b;

        vec4 rgbaOut = rgba;
        rgbaOut.g = updatedG;
        rgbaOut.b = updatedB;
        return rgbaOut;
    }
)";

const char* defaultVertexShader_glsl_130 =
    "in vec2 Position;\n"
    "in vec2 UV;\n"
    "out vec2 Frag_UV;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    gl_Position = vec4(Position.xy, 0, 1);\n"
    "}\n";
    
const char* defaultFragmentShader_glsl_130 =
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "out vec4 Out_Color;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = texture(Texture, Frag_UV.st);\n"
    "}\n";

const char* fragmentShader_Normal_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = texture(Texture, Frag_UV.st);
        Out_Color = srgb;
    }
)";

const char* fragmentShader_FlipRedBlue_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 rgba = RGB_from_SRGB(texture(Texture, Frag_UV.st));
        vec3 yCbCr = YCbCr_from_RGBA(rgba);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = yCbCr.z;
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGB_from_RGBClamped(RGBA_from_YCbCr (transformedYCbCr, 1.0));
    }
)";

const char* fragmentShader_FlipRedBlue_InvertRed_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 rgba = RGB_from_SRGB(texture(Texture, Frag_UV.st));
        vec3 yCbCr = YCbCr_from_RGBA(rgba);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = -yCbCr.z; // flip Cb
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGB_from_RGB(RGBA_from_YCbCr (transformedYCbCr, 1.0));
    }
)";

const char* fragmentShader_HSVTransform_glsl_130 = R"(
    uniform float u_hueShift;
    uniform float u_saturationScale;
    uniform int u_hueQuantization;

    uniform sampler2D Texture;
    in vec2 Frag_UV;
    out vec4 Out_Color;

    // http://www.workwithcolor.com/yellow-color-hue-range-01.htm
    vec2 quantizeHueLevel1 (int hue360)
    {
        if (hue360 < 10)  return vec2(0, 1);   // red
        if (hue360 < 20)  return vec2(15, 1);  // red-orange
        if (hue360 < 40)  return vec2(30, 1);  // orange/brown
        if (hue360 < 50)  return vec2(45, 1);  // orange-yellow
        if (hue360 < 60)  return vec2(60, 1);  // yellow
        if (hue360 < 80)  return vec2(70, 1);  // yellow green
        if (hue360 < 140) return vec2(110, 1); // green
        if (hue360 < 170) return vec2(125, 1); // green-cyan
        if (hue360 < 200) return vec2(185, 1); // cyan
        if (hue360 < 220) return vec2(210, 1); // cyan-blue
        if (hue360 < 240) return vec2(230, 1); // blue
        if (hue360 < 280) return vec2(260, 1); // blue-magenta
        if (hue360 < 320) return vec2(300, 1); // magenta
        if (hue360 < 330) return vec2(325, 1); // magenta-pink
        if (hue360 < 345) return vec2(338, 1); // pink
        if (hue360 < 355) return vec2(350, 1); // pink-red
        return vec2(0,1); // red again
    }

    // https://www.researchgate.net/figure/Nonuniform-hue-circle-quantization_fig6_224561621
    vec2 quantizeHueLevel2 (int hue360)
    {
        if (hue360 < 22)  return vec2(0, 1);   // red
        if (hue360 < 45)  return vec2(30, 1);  // orange
        if (hue360 < 70)  return vec2(60, 1);  // yellow
        if (hue360 < 155) return vec2(110, 1); // green
        if (hue360 < 186) return vec2(170, 1); // cyan
        if (hue360 < 278) return vec2(230, 1); // blue
        if (hue360 < 330) return vec2(300, 1); // purple
        return vec2(0,1); // red again
    }

    void main()
    {        
        vec4 srgba = texture(Texture, Frag_UV.st);
        // All in [0,1]
        vec3 hsv = HSV_from_SRGB(srgba.rgb);
        hsv.x = mod(hsv.x + u_hueShift, 1.0);
        if (u_hueQuantization != 0)
        { 
            vec2 hueBrightness = u_hueQuantization == 1 ? quantizeHueLevel1(int(hsv.x*360.0)) : quantizeHueLevel2(int(hsv.x*360.0));
            hsv.x = hueBrightness.x / 360.0;
            hsv.z = min(1.0, hsv.z * hueBrightness.y);
        }
        hsv.y = min(1.0, hsv.y * u_saturationScale);
        vec4 transformedSRGB = RGBA_from_HSV(hsv, 1.0);
        Out_Color = transformedSRGB;
    }
)";

const char* fragmentShader_DaltonizeV1_glsl_130 = R"(
    uniform sampler2D Texture;
    uniform int u_kind;
    uniform bool u_simulateOnly;
    uniform float u_severity;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 rgba = RGB_from_SRGB(texture(Texture, Frag_UV.st));
        vec3 lms = LMS_from_RGBA(rgba);
        vec3 lmsSimulated = vec3(0,1,0);
        switch (u_kind) {
            case 0: lmsSimulated = applyProtanope_Vienot(lms); break;
            case 1: lmsSimulated = applyDeuteranope_Vienot(lms); break;
            // Vienot 1999 is not accurate for tritanopia.
            case 2: lmsSimulated = applyTritanope_Brettel1997(lms); break;
        }
        // Linear interpolation to specify a similarity.
        lmsSimulated = mix(lms, lmsSimulated, u_severity);
        vec4 rgbaSimulated = RGBA_from_LMS(lmsSimulated, 1.0);
        vec4 rgbaOut = u_simulateOnly ? rgbaSimulated : daltonizeV1(rgba, rgbaSimulated);
        Out_Color = sRGB_from_RGBClamped(rgbaOut);
    }
)";

const char* fragmentShader_highlightSameColor = R"(
    uniform sampler2D Texture;
    uniform vec3 u_refColor_sRGB;
    uniform float u_deltaH_360;
    uniform float u_deltaS_100;
    uniform float u_deltaV_255;
    uniform int u_frameCount;
    uniform bool u_useRgbMaxDiff; // 0 means false, 1 means true
    in vec2 Frag_UV;
    out vec4 Out_Color;

    bool checkHSVDelta(vec3 hsv1, vec3 hsv2)
    {
        vec3 diff = abs(hsv1 - hsv2);
        diff.x = min (diff.x, 1.0-diff.x); // h is modulo 360º
        return (diff.x*360.0    < u_deltaH_360
                && diff.y*100.0 < u_deltaS_100
                && diff.z*255.0 < u_deltaV_255);
    }

    float max3 (vec3 v) { return max (max (v.x, v.y), v.z); }

    bool checkRGBDelta(vec3 rgb1, vec3 rgb2)
    {
        vec3 diff = abs(rgb1 - rgb2);
        return max3(diff)*255.0 < u_deltaV_255;
    }

    void main()
    {
        vec4 srgba = texture(Texture, Frag_UV.st);
        vec3 hsv = HSV_from_SRGB(srgba.rgb);
        
        bool isSame;
        if (u_useRgbMaxDiff)
        {
            isSame = checkRGBDelta (srgba.rgb, u_refColor_sRGB.rgb);
        }
        else
        {
            vec3 ref_hsv = HSV_from_SRGB(u_refColor_sRGB.rgb);
            isSame = checkHSVDelta(ref_hsv, hsv);
        }
                        
        float t = u_frameCount;
        float timeWeight = sin(t / 2.0)*0.5 + 0.5; // between 0 and 1
        float timeWeightedIntensity = timeWeight;
        hsv.z = mix (hsv.z, timeWeightedIntensity, isSame);

        vec4 transformedSRGB = RGBA_from_HSV(hsv, 1.0);
        Out_Color = transformedSRGB;
    }
)";

} // dl
