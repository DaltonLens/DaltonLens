//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "OpenGL.h"

#include <Dalton/Utils.h>
#include <Dalton/OpenGL_Shaders.h>

#include <gl3w/GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <vector>

namespace dl
{

void checkGLError ()
{
    GLenum err = glGetError();
    dl_assert (err == GL_NO_ERROR, "GL Error! 0x%x", (int)err);
}

const char* glslVersion()
{
    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    return "#version 150";
#else
    // GL 3.0 + GLSL 130
    return "#version 130";
#endif
}

} // dl

// --------------------------------------------------------------------------------
// GLShader
// --------------------------------------------------------------------------------

namespace dl
{

bool gl_checkProgram(GLint handle, const char* desc, const char* glsl_version)
{
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: GLShader: failed to link %s! (with GLSL '%s')\n", desc, glsl_version);
    if (log_length > 1)
    {
        std::vector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, NULL, (GLchar *)buf.data());
        fprintf(stderr, "%s\n", buf.data());
    }
    return (GLboolean)status == GL_TRUE;
}

bool gl_checkShader(GLuint handle, const char *desc)
{
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if ((GLboolean)status == GL_FALSE)
        fprintf(stderr, "ERROR: GLShader: failed to compile %s!\n", desc);
    if (log_length > 1)
    {
        std::vector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, NULL, (GLchar *)buf.data());
        fprintf(stderr, "%s\n", buf.data());
    }
    return (GLboolean)status == GL_TRUE;
}

struct GLShader::Impl
{
    GLint prevHandle = 0;
};

GLShader::GLShader()
: impl (new Impl())
{

}

GLShader::~GLShader()
{

}

void GLShader::initialize(const char* glslVersionString, const char* vertexShader, const char* fragmentShader)
{
    // Select shaders matching our GLSL versions
    if (vertexShader == nullptr)
        vertexShader = defaultVertexShader_glsl_130;
    
    if (fragmentShader == nullptr)
        fragmentShader = defaultFragmentShader_glsl_130;
    
    // Create shaders
    const GLchar *vertex_shader_with_version[3] = {glslVersionString, "\n", vertexShader};
    _glHandles.vertHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(_glHandles.vertHandle, 3, vertex_shader_with_version, NULL);
    glCompileShader(_glHandles.vertHandle);
    gl_checkShader(_glHandles.vertHandle, "vertex shader");
    
    const GLchar *fragment_shader_with_version[4] = {glslVersionString, "\n", commonFragmentLibrary, fragmentShader};
    _glHandles.fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(_glHandles.fragHandle, 4, fragment_shader_with_version, NULL);
    glCompileShader(_glHandles.fragHandle);
    gl_checkShader(_glHandles.fragHandle, "fragment shader");
    
    _glHandles.shaderHandle = glCreateProgram();
    glAttachShader(_glHandles.shaderHandle, _glHandles.vertHandle);
    glAttachShader(_glHandles.shaderHandle, _glHandles.fragHandle);
    glLinkProgram(_glHandles.shaderHandle);
    gl_checkProgram(_glHandles.shaderHandle, "shader program", glslVersionString);
    
    _glHandles.attribLocationTex = glGetUniformLocation(_glHandles.shaderHandle, "Texture");
    _glHandles.attribLocationProjMtx = glGetUniformLocation(_glHandles.shaderHandle, "ProjMtx");
    _glHandles.attribLocationVtxPos = (GLuint)glGetAttribLocation(_glHandles.shaderHandle, "Position");
    _glHandles.attribLocationVtxUV = (GLuint)glGetAttribLocation(_glHandles.shaderHandle, "UV");
    _glHandles.attribLocationVtxColor = (GLuint)glGetAttribLocation(_glHandles.shaderHandle, "Color");
    
    checkGLError();
}

void GLShader::enable ()
{
    dl_assert (_glHandles.shaderHandle != 0, "Forgot to call initialize()?");
    glGetIntegerv(GL_CURRENT_PROGRAM, &impl->prevHandle);
    glUseProgram (_glHandles.shaderHandle);
}

void GLShader::disable ()
{
    glUseProgram (impl->prevHandle);
    impl->prevHandle = 0;
}

} // dl

// --------------------------------------------------------------------------------
// GLTexture
// --------------------------------------------------------------------------------

namespace dl
{

GLTexture::~GLTexture()
{
    releaseGL();
}

void GLTexture::initializeWithExistingTextureID(uint32_t textureId)
{
    if (_textureId != 0)
        releaseGL();

    _textureId = textureId;

    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

void GLTexture::releaseGL()
{
    glDeleteTextures(1, &_textureId);
    _textureId = 0;
}

void GLTexture::initialize()
{
    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

    glGenTextures(1, &_textureId);
    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

void GLTexture::upload(const dl::ImageSRGBA& im)
{
    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(im.bytesPerRow() / im.bytesPerPixel()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im.width(), im.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

void GLTexture::setLinearInterpolationEnabled(bool enabled)
{
    if (_linearInterpolationEnabled == enabled)
        return;

    _linearInterpolationEnabled = enabled;
    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, enabled ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, enabled ? GL_LINEAR : GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

} // dl

// --------------------------------------------------------------------------------
// GLContext
// --------------------------------------------------------------------------------

namespace dl
{

struct GLContext::Impl
{
    GLFWwindow* window = nullptr;
};

GLContext::GLContext(GLContext* parentContext)
: impl (new Impl ())
{
    GLFWwindow* parentWindow = parentContext ? parentContext->impl->window : nullptr;
    glfwWindowHint(GLFW_VISIBLE, false);
    impl->window = glfwCreateWindow (16, 16, "offscreen context", nullptr, parentWindow);
    glfwWindowHint(GLFW_VISIBLE, true);
    glfwMakeContextCurrent (impl->window);
}

GLContext::~GLContext ()
{
    glfwDestroyWindow (impl->window);
}

void GLContext::makeCurrent ()
{
    glfwMakeContextCurrent (impl->window);
}

void GLContext::setCurrentToNull ()
{
    glfwMakeContextCurrent (nullptr);
}

} // dl