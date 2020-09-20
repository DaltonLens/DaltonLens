//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <metal_stdlib>
using namespace metal;

float3 yCbCrFromSRGBA (float4 srgba);
float4 sRGBAfromYCbCr (float3 yCbCr, float alpha);

float3 lmsFromSRGBA (float4 srgba);
float4 sRGBAFromLms (float3 lms, float alpha);

float3 HCVFromRGB(float3 RGB);
float3 RGBFromHUE(float H);

float3 HSLFromSRGBA(float4 srgba);
float4 sRGBAFromHSL(float3 HSL, float alpha);

float3 HSVFromSRGBA (float4 srgba);
float4 sRGBAFromHSV(float3 HSV, float alpha);

float3 applyProtanope (float3 lms);
float3 applyDeuteranope (float3 lms);
float3 applyTritanope (float3 lms);

float4 daltonizeV1 (float4 srgba, float4 srgbaSimulated);

// AA = antialiasing.
bool colorsAreCompatibleWithAA(float r0, float g0, float r1, float g1);

// Compared very bright colors is usually not meaningful, since any color will be almost
// white when antialiased with a white background.
bool canMakeSignificantComparisonBetweenAAColors (float r0, float g0, float r1, float g1);

constexpr sampler defaultSampler(coord::normalized,
                                 address::clamp_to_edge,
                                 filter::linear);

constant float hsxEpsilon = 1e-10;

struct Uniforms
{
    float4 srgbaUnderCursor;
    float4 grabScreenRectangle;
    int frameCount; // for animations
};

struct VertexIn
{
    float4 position [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
};

struct VertexOut
{
    float4 position [[position]] [[attribute(0)]];
    float2 texCoords [[attribute(1)]];
};

vertex VertexOut vertex_quad(VertexIn vert [[stage_in]],
                          uint vid [[vertex_id]])
{
    VertexOut out;
    out.position = vert.position;
    out.texCoords = vert.texCoords;
    return out;
}

// Adapted from http://www.chilliant.com/rgb2hsv.html

float3 HCVFromRGB(float3 RGB)
{
    // Based on work by Sam Hocevar and Emil Persson
    float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0/3.0) : float4(RGB.gb, 0.0, -1.0/3.0);
    float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6 * C + hsxEpsilon) + Q.z);
    return float3(H, C, Q.x);
}

float3 HSLFromSRGBA(float4 srgba)
{
    // FIXME: this is ignoring gamma and treating it like linearRGB
    
    float3 HCV = HCVFromRGB(srgba.rgb);
    float L = HCV.z - HCV.y * 0.5;
    float S = HCV.y / (1 - abs(L * 2 - 1) + hsxEpsilon);
    return float3(HCV.x, S, L);
}

float3 HSVFromSRGBA (float4 srgba)
{
    // FIXME: this is ignoring gamma and treating it like linearRGB
    
    float3 HCV = HCVFromRGB(srgba.xyz);
    float S = HCV.y / (HCV.z + hsxEpsilon);
    return float3(HCV.x, S, HCV.z);
}

float3 RGBFromHUE(float H)
{
    float R = abs(H * 6 - 3) - 1;
    float G = 2 - abs(H * 6 - 2);
    float B = 2 - abs(H * 6 - 4);
    return saturate(float3(R,G,B));
}

float4 sRGBAFromHSV(float3 HSV, float alpha)
{
    float3 RGB = RGBFromHUE(HSV.x);
    return float4(((RGB - 1) * HSV.y + 1) * HSV.z, alpha);
}

float4 sRGBAFromHSL(float3 HSL, float alpha)
{
    float3 RGB = RGBFromHUE(HSL.x);
    float C = (1 - abs(2 * HSL.z - 1)) * HSL.y;
    return float4((RGB - 0.5) * C + HSL.z, alpha);
}

float3 yCbCrFromSRGBA (float4 srgba)
{
    // FIXME: this is ignoring gamma and treating it like linearRGB
    
    float y =   0.57735027*srgba.r + 0.57735027*srgba.g + 0.57735027*srgba.b;
    float cr =  0.70710678*srgba.r - 0.70710678*srgba.g;
    float cb = -0.40824829*srgba.r - 0.40824829*srgba.g + 0.81649658*srgba.b;
    return float3(y,cb,cr);
}

float4 sRGBAfromYCbCr (float3 yCbCr, float alpha)
{
    // FIXME: this is ignoring gamma and treating it like linearRGB
    
    float4 srgbaOut;
    
    float y = yCbCr.x;
    float cb = yCbCr.y;
    float cr = yCbCr.z;
    
    srgbaOut.r = clamp(5.77350269e-01*y + 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
    srgbaOut.g = clamp(5.77350269e-01*y - 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
    srgbaOut.b = clamp(5.77350269e-01*y +            0.0*cr + 8.16496581e-01*cb, 0.0, 1.0);
    srgbaOut.a = alpha;
    return srgbaOut;
}

fragment float4 fragment_passthrough(VertexOut vert [[stage_in]],
                                     texture2d<float> screenTexture [[texture(0)]])
{
    float4 sampledColor = screenTexture.sample(defaultSampler, vert.texCoords);
    return sampledColor;
}

fragment float4 fragment_swapCbCr(VertexOut vert [[stage_in]],
                                  texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    
    float3 yCbCr = yCbCrFromSRGBA(srgba);
    float3 transformedYCbCr = yCbCr;
    transformedYCbCr.x = yCbCr.x;
    transformedYCbCr.y = yCbCr.z;
    transformedYCbCr.z = yCbCr.y;
    
    float4 srgbaOut = sRGBAfromYCbCr (transformedYCbCr, 1.0);
    return srgbaOut;
}

fragment float4 fragment_swapAndFlipCbCr(VertexOut vert [[stage_in]],
                                         texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    
    float3 yCbCr = yCbCrFromSRGBA(srgba);
    float3 transformedYCbCr = yCbCr;
    transformedYCbCr.x = yCbCr.x;
    transformedYCbCr.y = -yCbCr.z; // flip Cb
    transformedYCbCr.z = yCbCr.y;
    
    float4 srgbaOut = sRGBAfromYCbCr (transformedYCbCr, 1.0);
    return srgbaOut;
}

fragment float4 fragment_invertHue(VertexOut vert [[stage_in]],
                                   texture2d<float> screenTexture [[texture(0)]])
{
    // Invert the value component in HSV space (equivalent of the gimp value invert filter).
    // This tends to downweight colors with one strong RGB channel, where max(RGB) is high.
    
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 hsv = HSVFromSRGBA(srgba);
    hsv.z = 1.0 - hsv.z;
    float4 srgbaOut = sRGBAFromHSV (hsv, srgba.a);
    return srgbaOut;
}

fragment float4 fragment_invertLightness(VertexOut vert [[stage_in]],
                                         texture2d<float> screenTexture [[texture(0)]])
{
    // Invert the lightness component in HSL space. This tends to downweight
    // pure colors with very strong difference between channels, where min(RGB) is
    // small and max(RGB) is high.
    
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 hsl = HSLFromSRGBA(srgba);
    hsl.z = 1.0 - hsl.z;
    float4 srgbaOut = sRGBAFromHSL (hsl, srgba.a);
    return srgbaOut;
}

fragment float4 fragment_invertRGB(VertexOut vert [[stage_in]],
                                   texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float4 srgbaOut;
    srgbaOut.r = 1.0 - srgba.r;
    srgbaOut.g = 1.0 - srgba.g;
    srgbaOut.b = 1.0 - srgba.b;
    srgbaOut.a = srgba.a;
    return srgbaOut;
}

float3 lmsFromSRGBA (float4 srgba)
{
    //    17.8824, 43.5161, 4.11935,
    //    3.45565, 27.1554, 3.86714,
    //    0.0299566, 0.184309, 1.46709
    
    float3 lms;
    lms[0] = 17.8824*srgba.r + 43.5161*srgba.g + 4.11935*srgba.b;
    lms[1] = 3.45565*srgba.r + 27.1554*srgba.g + 3.86714*srgba.b;
    lms[2] = 0.0299566*srgba.r + 0.184309*srgba.g + 1.46709*srgba.b;
    return lms;
}

float4 sRGBAFromLms (float3 lms, float alpha)
{
    //    0.0809445    -0.130504     0.116721
    //    -0.0102485    0.0540193    -0.113615
    //    -0.000365297  -0.00412162     0.693511
    
    float4 srgbaOut;
    srgbaOut.r = clamp(0.0809445*lms[0] - 0.130504*lms[1] + 0.116721*lms[2], 0.0, 1.0);
    srgbaOut.g = clamp(-0.0102485*lms[0] + 0.0540193*lms[1] - 0.113615*lms[2], 0.0, 1.0);
    srgbaOut.b = clamp(-0.000365297*lms[0] - 0.00412162*lms[1] + 0.693511*lms[2], 0.0, 1.0);
    srgbaOut.a = alpha;
    return srgbaOut;
}

float3 applyProtanope (float3 lms)
{
    //    p.l = /* 0*p.l + */ 2.02344*p.m - 2.52581*p.s;
    float3 lmsTransformed = lms;
    lmsTransformed[0] = 2.02344*lms[1] - 2.52581*lms[2];
    return lmsTransformed;
}

float3 applyDeuteranope (float3 lms)
{
    //    p.m = 0.494207*p.l /* + 0*p.m */ + 1.24827*p.s;
    float3 lmsTransformed = lms;
    lmsTransformed[1] = 0.494207*lms[0] + 1.24827*lms[2];
    return lmsTransformed;
}

float3 applyTritanope (float3 lms)
{
    //    p.s = -0.395913*p.l + 0.801109*p.m /* + 0*p.s */;
    float3 lmsTransformed = lms;
    lmsTransformed[2] = -0.395913*lms[0] + 0.801109*lms[1];
    return lmsTransformed;
}

fragment float4 fragment_simulateDaltonism_protanope(VertexOut vert [[stage_in]],
                                                     texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsTransformed = applyProtanope(lms);
    float4 srgbaOut = sRGBAFromLms(lmsTransformed, 1.0);
    return srgbaOut;
}

fragment float4 fragment_simulateDaltonism_deuteranope(VertexOut vert [[stage_in]],
                                                       texture2d<float> screenTexture [[texture(0)]])
{
    
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsTransformed = applyDeuteranope(lms);
    float4 srgbaOut = sRGBAFromLms(lmsTransformed, 1.0);
    return srgbaOut;
}

fragment float4 fragment_simulateDaltonism_tritanope(VertexOut vert [[stage_in]],
                                                     texture2d<float> screenTexture [[texture(0)]])
{
    
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsTransformed = applyTritanope(lms);
    float4 srgbaOut = sRGBAFromLms(lmsTransformed, 1.0);
    return srgbaOut;
}

float4 daltonizeV1 (float4 srgba, float4 srgbaSimulated)
{
    float3 error = srgba.rgb - srgbaSimulated.rgb;
    
    float updatedG = srgba.g + 0.7*error.r + 1.0*error.g + 0.0*error.b;
    float updatedB = srgba.b + 0.7*error.r + 0.0*error.g + 1.0*error.b;
    
    float4 srgbaOut = srgba;
    srgbaOut.g = clamp(updatedG, 0.0, 1.0);
    srgbaOut.b = clamp(updatedB, 0.0, 1.0);
    return srgbaOut;
}

fragment float4 fragment_daltonizeV1_protanope(VertexOut vert [[stage_in]],
                                               texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsSimulated = applyProtanope(lms);
    float4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
    float4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
    return srgbaOut;
}

fragment float4 fragment_daltonizeV1_deuteranope(VertexOut vert [[stage_in]],
                                                 texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsSimulated = applyDeuteranope(lms);
    float4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
    float4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
    return srgbaOut;
}

fragment float4 fragment_daltonizeV1_tritanope(VertexOut vert [[stage_in]],
                                               texture2d<float> screenTexture [[texture(0)]])
{
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 lms = lmsFromSRGBA(srgba);
    float3 lmsSimulated = applyTritanope(lms);
    float4 srgbaSimulated = sRGBAFromLms(lmsSimulated, 1.0);
    float4 srgbaOut = daltonizeV1(srgba, srgbaSimulated);
    return srgbaOut;
}

bool colorsAreCompatibleWithAA(float r0, float g0, float r1, float g1)
{
    // FIXME: document this. This comes from some python experiments.
    // If the true color is c, then the final antialised color will be a blend
    // with the background. Assuming the background is white, we get a linear
    // constraint between the components. This checks is the constraints are
    // within range.
    float rg0 = (255.0-r0)/(255.0-g0);
    float rg1 = (255.0-r1)/(255.0-g1);
    float errRg0 = ((256.0-r0)/(255.0-g0)) - ((255.0-r0)/(256.0-g0));
    float errRg1 = ((256.0-r1)/(255.0-g1)) - ((255.0-r1)/(256.0-g1));
    float deltaRg = abs(rg0-rg1);
    bool cantTell = (g1 > 220);
    bool colorAreCompatible = (deltaRg < 6.0*(errRg0+errRg1));
    bool compatible = colorAreCompatible || cantTell;
    return compatible;
}

bool canMakeSignificantComparisonBetweenAAColors (float r0, float g0, float r1, float g1)
{
    // all the colors look similar when strongly blended with a white background.
    return (g1 < 200.0);
}

fragment float4 fragment_highlightSameColorWithAntialising(VertexOut vert [[stage_in]],
                                                           texture2d<float> screenTexture [[texture(0)]],
                                                           constant Uniforms &uniforms [[buffer(0)]])
{
    // FIXME: document this.
    float4 srgbaToHighlight = uniforms.srgbaUnderCursor;
    float3 yCbCrToHightlight = yCbCrFromSRGBA(srgbaToHighlight);
    float r0 = 255.0*srgbaToHighlight.r;
    float g0 = 255.0*srgbaToHighlight.g;
    float b0 = 255.0*srgbaToHighlight.b;
    
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    float3 yCbCr = yCbCrFromSRGBA(srgba);
    float r1 = 255.0*srgba.r;
    float g1 = 255.0*srgba.g;
    float b1 = 255.0*srgba.b;
    
    // NEXT IDEA: local neighborhood. Accept pixel if ok similariy if at least one neighbor with very good similarity. Nearest pixel sampling.
    
    // Check if all the channels satisfy the linear constraints of a single true color
    // blended with a white background due to antialiasing.
    bool compatibleColor = true;
    compatibleColor &= colorsAreCompatibleWithAA(r0,g0,r1,g1);
    compatibleColor &= colorsAreCompatibleWithAA(g0,r0,g1,r1);
    compatibleColor &= colorsAreCompatibleWithAA(r0,b0,r1,b1);
    compatibleColor &= colorsAreCompatibleWithAA(b0,r0,b1,r1);
    compatibleColor &= colorsAreCompatibleWithAA(g0,b0,g1,b1);
    compatibleColor &= colorsAreCompatibleWithAA(b0,g0,b1,g1);
    
    int numSignificant = (canMakeSignificantComparisonBetweenAAColors(r0,g0,r1,g1)
                          + canMakeSignificantComparisonBetweenAAColors(g0,r0,g1,r1)
                          + canMakeSignificantComparisonBetweenAAColors(r0,b0,r1,b1)
                          + canMakeSignificantComparisonBetweenAAColors(b0,r0,b1,r1)
                          + canMakeSignificantComparisonBetweenAAColors(g0,b0,g1,b1)
                          + canMakeSignificantComparisonBetweenAAColors(b0,g0,b1,g1));
                          
    bool isSignificantTest = (numSignificant>=1);
    
    bool isSameColor = isSignificantTest && compatibleColor;

    float finalWeight = select (0.0, 1.0, isSameColor); // 1.0 if isSameColor
    
    // Apply time-based weighting
    float t = uniforms.frameCount;
    float timeWeight = sin(t / 1.0)*0.5 + 0.5; // between 0 and 1
    
    finalWeight = (finalWeight*timeWeight + 1.0)/2.0; // between 0 and 1, min 0.5
    
    // Don't highlight gray stuff.
    bool isGray = length(yCbCr.yz) < 0.01;
    finalWeight = select (finalWeight, 1.0, isGray);
    
    float3 weightedSRGB = srgba.rgb * finalWeight;
    
    bool shouldHighlight = length(yCbCrToHightlight.yz) > 0.01;
    float3 finalSRGB = select (srgba.rgb, weightedSRGB, shouldHighlight);
    return float4(finalSRGB, 1.0);
}

fragment float4 fragment_highlightSameColor(VertexOut vert [[stage_in]],
                                            texture2d<float> screenTexture [[texture(0)]],
                                            constant Uniforms &uniforms [[buffer(0)]])
{
    float4 srgbaToHighlight = uniforms.srgbaUnderCursor;
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
    
    // Compare the current color with the reference one.
    bool isSame = length(srgbaToHighlight.rgb - srgba.rgb) < 0.01;
    
    // FIXME: document this better.
    
    float3 yCbCr = yCbCrFromSRGBA(srgba);
    yCbCr.yz = select (float2(0,0), yCbCr.yz, isSame);
    
    float t = uniforms.frameCount;
    float timeWeight = sin(t / 2.0)*0.5 + 0.5; // between 0 and 1
    timeWeight = select (timeWeight*0.5, -timeWeight*0.8, yCbCr.x > 0.86);
    float timeWeightedIntensity = yCbCr.x + timeWeight;
    yCbCr.x = select (yCbCr.x, timeWeightedIntensity, isSame);

    float3 yCbCrToHightlight = yCbCrFromSRGBA(srgbaToHighlight);
    bool shouldHighlight = length(yCbCrToHightlight.yz) > 0.01;
    
    float3 transformedSRGB = sRGBAfromYCbCr(yCbCr, 1.0).rgb;
    
    float3 finalSRGB = select (srgba.rgb, transformedSRGB, shouldHighlight);
    return float4(finalSRGB, 1.0);
}

fragment float4 fragment_grabScreenArea(VertexOut vert [[stage_in]],
                                        texture2d<float> screenTexture [[texture(0)]],
                                        constant Uniforms &uniforms [[buffer(0)]])
{
    float4 rect = uniforms.grabScreenRectangle;
    float4 srgba = screenTexture.sample(defaultSampler, vert.texCoords);
        
    float xMin = rect[0];
    float yMin = rect[1];
    float xMax = rect[2];
    float yMax = rect[3];
    
    bool isInRect = (vert.texCoords.x >= xMin && vert.texCoords.x <= xMax) && (vert.texCoords.y >= yMin && vert.texCoords.y <= yMax);
    // bool isInRect = (vert.texCoords.x >= 0.2 && vert.texCoords.x <= 0.4) && (vert.texCoords.y >= 0.2 && vert.texCoords.y <= 0.4);
    
    float3 yCbCr = yCbCrFromSRGBA(srgba);
    yCbCr.x = select (yCbCr.x, yCbCr.x*0.5, !isInRect);
    float3 maybeTransformedSRGB = sRGBAfromYCbCr(yCbCr, 1.0).rgb;
    
    float3 finalSRGB = maybeTransformedSRGB;
    return float4(finalSRGB, 1.0);
}

