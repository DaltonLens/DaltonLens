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

    // https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    // Converts a color from linear light gamma to sRGB gamma
    vec4 fromLinear(vec4 linearRGB)
    {
        bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));
        vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);
        vec3 lower = linearRGB.rgb * vec3(12.92);

        return vec4(mix(higher, lower, cutoff), linearRGB.a);
    }

    vec4 fromLinearClamped (vec4 rgba)
    {
        return fromLinear(clamp(rgba, 0.0, 1.0));
    }

    // Converts a color from sRGB gamma to linear light gamma
    vec4 toLinear(vec4 sRGB)
    {
        bvec3 cutoff = lessThan(sRGB.rgb, vec3(0.04045));
        vec3 higher = pow((sRGB.rgb + vec3(0.055))/vec3(1.055), vec3(2.4));
        vec3 lower = sRGB.rgb/vec3(12.92);

        return vec4(mix(higher, lower, cutoff), sRGB.a);
    }

    vec3 yCbCrFromRGBA (vec4 rgba)
    {
        float y =   0.57735027*rgba.r + 0.57735027*rgba.g + 0.57735027*rgba.b;
        float cr =  0.70710678*rgba.r - 0.70710678*rgba.g;
        float cb = -0.40824829*rgba.r - 0.40824829*rgba.g + 0.81649658*rgba.b;
        return vec3(y,cb,cr);
    }

    vec4 RGBAfromYCbCr (vec3 yCbCr, float alpha)
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

    vec3 lmsFromRGBA (vec4 rgba)
    {
        // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.LMS_from_linearRGB

        vec3 lms;
        lms[0] = 0.1789*rgba.r + 0.44  *rgba.g + 0.036 *rgba.b;
        lms[1] = 0.0338*rgba.r + 0.2752*rgba.g + 0.0362*rgba.b;
        lms[2] = 0.0003*rgba.r + 0.0019*rgba.g + 0.0153*rgba.b;
        return lms;
    }

    vec4 RGBAFromLms (vec3 lms, float alpha)
    {
        // DaltonLens-Python LMSModel_sRGB_SmithPokorny75.linearRGB_from_LMS
    
        vec4 rgbaOut;
        rgbaOut.r =  8.0053*lms[0] - 12.882*lms[1] + 11.6806 * lms[2];
        rgbaOut.g = -0.9782*lms[0] + 5.2694*lms[1] - 10.183  * lms[2];
        rgbaOut.b = -0.0402*lms[0] - 0.3989*lms[1] + 66.4808 * lms[2];
        rgbaOut.a = alpha;
        return rgbaOut;
    }

    // H in [0,1]
    // C in [0,1]
    // V in [0,1]
    vec3 HCVFromRGB(vec3 RGB)
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
    vec3 HSVFromRGB (vec3 rgb)
    {
        // FIXME: this is ignoring gamma and treating it like linearRGB
    
        vec3 HCV = HCVFromRGB(rgb.xyz);
        float S = HCV.y / (HCV.z + hsxEpsilon);
        return vec3(HCV.x, S, HCV.z);
    }

    vec3 RGBFromHUE(float H)
    {
        float R = abs(H * 6 - 3) - 1;
        float G = 2 - abs(H * 6 - 2);
        float B = 2 - abs(H * 6 - 4);
        return clamp(vec3(R,G,B), 0, 1);
    }

    vec4 RGBAFromHSV(vec3 HSV, float alpha)
    {
        vec3 RGB = RGBFromHUE(HSV.x);
        return vec4(((RGB - 1) * HSV.y + 1) * HSV.z, alpha);
    }

    vec3 applyProtanope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        vec3 lmsTransformed = lms;
        lmsTransformed[0] = 2.0205*lms[1] - 2.4337*lms[2];
        return lmsTransformed;
    }

    vec3 applyDeuteranope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        vec3 lmsTransformed = lms;
        lmsTransformed[1] = 0.4949*lms[0] + 1.2045*lms[2];
        return lmsTransformed;
    }

    vec3 applyTritanope_Vienot (vec3 lms)
    {
        // DaltonLens-Python Simulator_Vienot1999.lms_projection_matrix
        // WARNING: Viénot is not good for tritanopia, we need
        // to switch to Brettel.
        vec3 lmsTransformed = lms;
        lmsTransformed[2] = -0.0122*lms[0] + 0.0739*lms[1];
        return lmsTransformed;
    }

    vec4 daltonizeV1 (vec4 rgba, vec4 rgbaSimulated)
    {
        vec3 error = rgba.rgb - rgbaSimulated.rgb;

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
        vec4 rgba = toLinear(texture(Texture, Frag_UV.st));
        vec3 yCbCr = yCbCrFromRGBA(rgba);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = yCbCr.z;
        transformedYCbCr.z = yCbCr.y;
        Out_Color = fromLinearClamped(RGBAfromYCbCr (transformedYCbCr, 1.0));
    }
)";

const char* fragmentShader_FlipRedBlue_InvertRed_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 rgba = toLinear(texture(Texture, Frag_UV.st));
        vec3 yCbCr = yCbCrFromRGBA(rgba);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = -yCbCr.z; // flip Cb
        transformedYCbCr.z = yCbCr.y;
        Out_Color = fromLinear(RGBAfromYCbCr (transformedYCbCr, 1.0));
    }
)";

const char* fragmentShader_DaltonizeV1_glsl_130 = R"(
    uniform sampler2D Texture;
    uniform int u_kind;
    uniform bool u_simulateOnly;
    in vec2 Frag_UV;
    out vec4 Out_Color;
    void main()
    {
        vec4 rgba = toLinear(texture(Texture, Frag_UV.st));
        vec3 lms = lmsFromRGBA(rgba);
        vec3 lmsSimulated = vec3(0,1,0);
        switch (u_kind) {
            case 0: lmsSimulated = applyProtanope_Vienot(lms); break;
            case 1: lmsSimulated = applyDeuteranope_Vienot(lms); break;
            case 2: lmsSimulated = applyTritanope_Vienot(lms); break;
        }
        vec4 rgbaSimulated = RGBAFromLms(lmsSimulated, 1.0);
        vec4 rgbaOut = u_simulateOnly ? rgbaSimulated : daltonizeV1(rgba, rgbaSimulated);
        Out_Color = fromLinearClamped(rgbaOut);
    }
)";

const char* fragmentShader_highlightSameColor = R"(
    uniform sampler2D Texture;
    uniform vec3 u_refColor;
    uniform float u_deltaH_360;
    uniform float u_deltaS_100;
    uniform float u_deltaV_255;
    uniform int u_frameCount;
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

    void main()
    {
        vec4 rgba = toLinear(texture(Texture, Frag_UV.st));
        vec3 hsv = HSVFromRGB(rgba.rgb);                
        vec3 ref_hsv = HSVFromRGB(u_refColor.rgb);
        
        bool isSame = checkHSVDelta(ref_hsv, hsv);
                        
        float t = u_frameCount;
        float timeWeight = sin(t / 2.0)*0.5 + 0.5; // between 0 and 1
        float timeWeightedIntensity = timeWeight;
        hsv.z = mix (hsv.z, timeWeightedIntensity, isSame);

        vec4 transformedRGB = RGBAFromHSV(hsv, 1.0);
        Out_Color = fromLinearClamped(transformedRGB);
    }
)";

} // dl
