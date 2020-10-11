//
//  Graphics.hpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
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
    void initialize ();

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
