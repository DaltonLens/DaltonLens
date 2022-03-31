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
    uint32_t shaderHandle = 0;
    uint32_t vertHandle = 0;
    uint32_t fragHandle = 0;
    uint32_t textureUniformLocation = 0;
};
class GLShader
{
public:
    // Predefined attribute locations meant to be enforced via
    // glbindattriblocation so we can switch shaders without having
    // to adjust the rendering code.

    enum class Attribute : uint32_t { 
        VertexPos    = 0,
        VertexNormal = 1,
        VertexColor  = 2,
        VertexUV     = 3,
    };

public:
    GLShader();
    ~GLShader();
    
    void initialize (const char* glslVersionString, const char* vertexShader, const char* fragmentShader);

    void enable (int32_t textureId = 0);
    void disable ();

    const GLShaderHandles& glHandles () const { return _glHandles; }

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;

    GLShaderHandles _glHandles;
};

struct GLRestoreStateAfterScope_Texture
{
    GLRestoreStateAfterScope_Texture();
    ~GLRestoreStateAfterScope_Texture();

private:
    int32_t _prevTexture = 0;
};
class GLTexture
{
public:
    ~GLTexture();
    
    int width () const { return _width; }
    int height () const { return _height; }    

    void initialize ();
    void initializeWithExistingTextureID (uint32_t textureId, int width, int height);
    bool isInitialized () const { return _textureId > 0; }
    void releaseGL ();

    void ensureAllocatedForRGBA (int width, int height);
    void upload (const dl::ImageSRGBA& im);
    void uploadRgba(const uint8_t* rgbaBuffer, int width, int height, int bytesPerRow = -1);
    void download (dl::ImageSRGBA& im);

    uint32_t textureId() const { return _textureId; }

    void setLinearInterpolationEnabled (bool enabled);

private:
    uint32_t _textureId = 0;
    bool _linearInterpolationEnabled = false;
    int _width = 0;
    int _height = 0;
};

// Offscreen GL context.
class GLContext
{
public:
    GLContext(GLContext* parentContext = nullptr);
    ~GLContext ();

    void makeCurrent ();

public:
    static void setCurrentToNull ();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

class GLFrameBuffer
{
public:
    GLFrameBuffer ();
    ~GLFrameBuffer ();

public:
    void enable (int width, int height);
    void disable ();
    void downloadBuffer (ImageSRGBA& output) const;
    GLTexture& outputColorTexture ();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

class GLImageRenderer
{
public:
    ~GLImageRenderer ();

    void initializeGL ();
    void render ();

private:
    uint32_t _vbo = 0;
    uint32_t _vao = 0;
    uint32_t _elementbuffer = 0;
};

} // dl
