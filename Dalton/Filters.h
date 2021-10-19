//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/Image.h>

#include <string>

namespace dl
{

struct GLShaderHandles;
class GLShader;
class GLTexture;

class GLFilter
{
public:
    GLFilter ();
    virtual ~GLFilter();

public:
    // Default implementation is an assert.
    virtual void applyCPU (const ImageSRGBA& input, ImageSRGBA& output) const;

public:
    // GPU methods.
    virtual void initializeGL () = 0;
    virtual void enableGLShader ();
    virtual void disableGLShader ();
    const GLShaderHandles &glHandles() const;
    GLShader* glShader () const;

protected:
    void initializeGL(const char* glslVersionString, const char* vertexShader, const char* fragmentShader);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

class GLFilterProcessor
{
public:
    GLFilterProcessor();
    ~GLFilterProcessor();

public:
    void initializeGL ();
    void render (GLFilter& filter, uint32_t inputTextureId, int width, int height, ImageSRGBA* output = nullptr);
    GLTexture& filteredTexture();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

class Filter_FlipRedBlue : public GLFilter
{
public:
    virtual void initializeGL () override;
};

class Filter_FlipRedBlueAndInvertRed : public GLFilter
{
public:
    virtual void initializeGL () override;
};

class Filter_Daltonize : public GLFilter
{
public:
    struct Params
    {
        enum Kind
        {
            Protanope   = 0,
            Deuteranope = 1,
            Tritanope   = 2,
        } kind = Protanope;

        bool simulateOnly = false;
    };

public:
    void setParams (const Params& params) { _currentParams = params; }

public:
    virtual void initializeGL () override;
    virtual void enableGLShader () override;
    virtual void applyCPU (const ImageSRGBA& input, ImageSRGBA& output) const override;

private:
    Params _currentParams;
    unsigned _attribLocationKind = 0;
    unsigned _attribLocationSimulateOnly = 0;
};

class Filter_HighlightSimilarColors : public GLFilter
{
public:
    struct Params
    {
        bool hasActiveColor = false;
        vec4d activeColorRGB01 = vec4d(0,0,0,1);
        float deltaH_360 = NAN; // within [0,360º]
        float deltaS_100 = NAN; // within [0,100%]
        float deltaV_255 = NAN; // within [0,255]
        int frameCount = 0;
    };

public:
    void setParams (const Params& params) { _currentParams = params;}

public:
    virtual void initializeGL () override;
    virtual void enableGLShader () override;

private:
    Params _currentParams;
    unsigned _attribLocationRefColor_linearRGB = 0;
    unsigned _attribLocationDeltaH = 0;
    unsigned _attribLocationDeltaS = 0;
    unsigned _attribLocationDeltaV = 0;
    unsigned _attribLocationFrameCount = 0;
};

} // dl