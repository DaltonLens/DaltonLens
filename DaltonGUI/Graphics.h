//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/Image.h>

namespace dl
{

void checkGLError ();

const char* glslVersion();

class GLTexture
{
public:
    ~GLTexture();
    
    void initialize ();
    void initializeWithExistingTextureID (uint32_t textureId);
    void releaseGL ();

    void upload (const dl::ImageSRGBA& im);

    uint32_t textureId() const { return _textureId; }

private:
    uint32_t _textureId = 0;
};

} // dl

namespace dl
{

class GLShader
{
public:
    GLShader();
    ~GLShader();
    
    const std::string& name() const;
    
    void initialize(const std::string& name, const char* glslVersionString, const char* vertexShader, const char* fragmentShader);
    
    uint32_t shaderHandle() const;

    void setExtraUserCallback (const std::function<void(GLShader&)>& extraRenderCallback);
    
    void enable ();

    void disable ();

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};


} // dl
