//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/Image.h>

#include <memory>
#include <cstdint>

namespace dl
{

void checkGLError ();

const char* glslVersion();

struct GLShaderHandles
{
    unsigned shaderHandle = 0;
    unsigned vertHandle = 0;
    unsigned fragHandle = 0;
    unsigned attribLocationTex = 0;
    unsigned attribLocationProjMtx = 0;
    unsigned attribLocationVtxPos = 0;
    unsigned attribLocationVtxUV = 0;
    unsigned attribLocationVtxColor = 0;
};
class GLShader
{    
public:
    GLShader();
    ~GLShader();
    
    void initialize(const char* glslVersionString, const char* vertexShader, const char* fragmentShader);

    void enable ();
    void disable ();

    const GLShaderHandles& glHandles () const { return _glHandles; }

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;

    GLShaderHandles _glHandles;
};

class GLTexture
{
public:
    ~GLTexture();
    
    void initialize ();
    void initializeWithExistingTextureID (uint32_t textureId);
    void releaseGL ();

    void upload (const dl::ImageSRGBA& im);

    uint32_t textureId() const { return _textureId; }

    void setLinearInterpolationEnabled (bool enabled);

private:
    uint32_t _textureId = 0;
    bool _linearInterpolationEnabled = false;
};

// Offscreen GL context.
class GLContext
{
public:
    GLContext(GLContext* parentContext);
    ~GLContext ();

    void makeCurrent ();

public:
    static void setCurrentToNull ();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
