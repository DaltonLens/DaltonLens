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

    vec3 yCbCrFromSRGBA (vec4 srgba)
    {
        // FIXME: this is ignoring gamma and treating it like linearRGB
        float y =   0.57735027*srgba.r + 0.57735027*srgba.g + 0.57735027*srgba.b;
        float cr =  0.70710678*srgba.r - 0.70710678*srgba.g;
        float cb = -0.40824829*srgba.r - 0.40824829*srgba.g + 0.81649658*srgba.b;
        return vec3(y,cb,cr);
    }

    vec4 sRGBAfromYCbCr (vec3 yCbCr, float alpha)
    {
        // FIXME: this is ignoring gamma and treating it like linearRGB
        
        vec4 srgbaOut;
        
        float y = yCbCr.x;
        float cb = yCbCr.y;
        float cr = yCbCr.z;
        
        srgbaOut.r = clamp(5.77350269e-01*y + 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
        srgbaOut.g = clamp(5.77350269e-01*y - 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
        srgbaOut.b = clamp(5.77350269e-01*y +            0.0*cr + 8.16496581e-01*cb, 0.0, 1.0);
        srgbaOut.a = alpha;
        return srgbaOut;
    }

    vec3 lmsFromSRGBA (vec4 srgba)
    {
        //    17.8824, 43.5161, 4.11935,
        //    3.45565, 27.1554, 3.86714,
        //    0.0299566, 0.184309, 1.46709
    
        vec3 lms;
        lms[0] = 17.8824*srgba.r + 43.5161*srgba.g + 4.11935*srgba.b;
        lms[1] = 3.45565*srgba.r + 27.1554*srgba.g + 3.86714*srgba.b;
        lms[2] = 0.0299566*srgba.r + 0.184309*srgba.g + 1.46709*srgba.b;
        return lms;
    }

    vec4 sRGBAFromLms (vec3 lms, float alpha)
    {
        //    0.0809445    -0.130504     0.116721
        //    -0.0102485    0.0540193    -0.113615
        //    -0.000365297  -0.00412162     0.693511
    
        vec4 srgbaOut;
        srgbaOut.r = clamp(0.0809445*lms[0] - 0.130504*lms[1] + 0.116721*lms[2], 0.0, 1.0);
        srgbaOut.g = clamp(-0.0102485*lms[0] + 0.0540193*lms[1] - 0.113615*lms[2], 0.0, 1.0);
        srgbaOut.b = clamp(-0.000365297*lms[0] - 0.00412162*lms[1] + 0.693511*lms[2], 0.0, 1.0);
        srgbaOut.a = alpha;
        return srgbaOut;
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
    vec3 HSVFromSRGB (vec3 srgb)
    {
        // FIXME: this is ignoring gamma and treating it like linearRGB
    
        vec3 HCV = HCVFromRGB(srgb.xyz);
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

    vec4 sRGBAFromHSV(vec3 HSV, float alpha)
    {
        vec3 RGB = RGBFromHUE(HSV.x);
        return vec4(((RGB - 1) * HSV.y + 1) * HSV.z, alpha);
    }

    vec3 applyProtanope (vec3 lms)
    {
        //    p.l = /* 0*p.l + */ 2.02344*p.m - 2.52581*p.s;
        vec3 lmsTransformed = lms;
        lmsTransformed[0] = 2.02344*lms[1] - 2.52581*lms[2];
        return lmsTransformed;
    }

    vec3 applyDeuteranope (vec3 lms)
    {
        //    p.m = 0.494207*p.l /* + 0*p.m */ + 1.24827*p.s;
        vec3 lmsTransformed = lms;
        lmsTransformed[1] = 0.494207*lms[0] + 1.24827*lms[2];
        return lmsTransformed;
    }

    vec3 applyTritanope (vec3 lms)
    {
        //    p.s = -0.395913*p.l + 0.801109*p.m /* + 0*p.s */;
        vec3 lmsTransformed = lms;
        lmsTransformed[2] = -0.395913*lms[0] + 0.801109*lms[1];
        return lmsTransformed;
    }

    vec4 daltonizeV1 (vec4 srgba, vec4 srgbaSimulated)
    {
        vec3 error = srgba.rgb - srgbaSimulated.rgb;

        float updatedG = srgba.g + 0.7*error.r + 1.0*error.g + 0.0*error.b;
        float updatedB = srgba.b + 0.7*error.r + 0.0*error.g + 1.0*error.b;

        vec4 srgbaOut = srgba;
        srgbaOut.g = clamp(updatedG, 0.0, 1.0);
        srgbaOut.b = clamp(updatedB, 0.0, 1.0);
        return srgbaOut;
    }
)";

const char* defaultVertexShader_glsl_130 =
    "uniform mat4 ProjMtx;\n"
    "in vec2 Position;\n"
    "in vec2 UV;\n"
    "in vec4 Color;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";
    
const char* defaultFragmentShader_glsl_130 =
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "out vec4 Out_Color;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

const char* fragmentShader_Normal_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        Out_Color = vec4(srgb.r, srgb.g, srgb.b, srgb.a);
    }
)";

const char* fragmentShader_FlipRedBlue_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 yCbCr = yCbCrFromSRGBA(srgb);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = yCbCr.z;
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGBAfromYCbCr (transformedYCbCr, 1.0);
    }
)";

const char* fragmentShader_FlipRedBlue_InvertRed_glsl_130 = R"(
    uniform sampler2D Texture;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgb = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 yCbCr = yCbCrFromSRGBA(srgb);
        vec3 transformedYCbCr = yCbCr;
        transformedYCbCr.x = yCbCr.x;
        transformedYCbCr.y = -yCbCr.z; // flip Cb
        transformedYCbCr.z = yCbCr.y;
        Out_Color = sRGBAfromYCbCr (transformedYCbCr, 1.0);
    }
)";

const char* fragmentShader_DaltonizeV1_glsl_130 = R"(
    uniform sampler2D Texture;
    uniform int u_kind;
    uniform bool u_simulateOnly;
    in vec2 Frag_UV;
    in vec4 Frag_Color;
    out vec4 Out_Color;
    void main()
    {
        vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 lms = lmsFromSRGBA(srgba);
        vec3 lmsSimulated = vec3(0,1,0);
        switch (u_kind) {
            case 0: lmsSimulated = applyProtanope(lms); break;
            case 1: lmsSimulated = applyDeuteranope(lms); break;
            case 2: lmsSimulated = applyTritanope(lms); break;
        }
        vec4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
        vec4 srgbaOut = u_simulateOnly ? srgbaSimulated : daltonizeV1(srgba, srgbaSimulated);
        Out_Color = srgbaOut;
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
    in vec4 Frag_Color;
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
        vec4 srgba = Frag_Color * texture(Texture, Frag_UV.st);
        vec3 hsv = HSVFromSRGB(srgba.rgb);                
        vec3 ref_hsv = HSVFromSRGB(u_refColor.rgb);
        
        bool isSame = checkHSVDelta(ref_hsv, hsv);
                        
        float t = u_frameCount;
        float timeWeight = sin(t / 2.0)*0.5 + 0.5; // between 0 and 1
        float timeWeightedIntensity = timeWeight;
        hsv.z = mix (hsv.z, timeWeightedIntensity, isSame);

        vec4 transformedSRGB = sRGBAFromHSV(hsv, 1.0);
        Out_Color = transformedSRGB;
    }
)";

} // dl
