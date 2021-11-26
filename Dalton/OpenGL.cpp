//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "OpenGL.h"

#include <Dalton/Platform.h>
#include <Dalton/Utils.h>
#include <Dalton/OpenGL_Shaders.h>

#include <gl3w/GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <array>
#include <numeric>

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
#if PLATFORM_MACOS
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

// https://stackoverflow.com/questions/13172158/c-split-string-by-line/13172579
std::vector<std::string> split_string(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> strings;
 
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != std::string::npos)
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }
 
    // To get the last substring (or only, if delimiter is not found)
    strings.push_back(str.substr(prev));
    return strings;
}

void showSourceCodeWithLines (const GLchar *shader_code[], int numElements)
{
    fprintf (stderr, "Source code:\n");
    std::string source_code = std::accumulate (shader_code, shader_code + numElements, std::string ());
    std::vector<std::string> source_lines = split_string (source_code, "\n");
    for (int i = 0; i < source_lines.size (); ++i)
    {
        fprintf (stderr, "%04d %s\n", i, source_lines[i].c_str ());
    }
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
    if (!gl_checkShader(_glHandles.fragHandle, "fragment shader"))
    {
        showSourceCodeWithLines (fragment_shader_with_version, sizeof(fragment_shader_with_version)/sizeof(GLchar*));
    }
    
    _glHandles.shaderHandle = glCreateProgram();
    glAttachShader(_glHandles.shaderHandle, _glHandles.vertHandle);
    glAttachShader(_glHandles.shaderHandle, _glHandles.fragHandle);
    glBindAttribLocation(_glHandles.shaderHandle, (GLuint)Attribute::VertexPos,    "Position");
    glBindAttribLocation(_glHandles.shaderHandle, (GLuint)Attribute::VertexNormal, "Normal");
    glBindAttribLocation(_glHandles.shaderHandle, (GLuint)Attribute::VertexUV,     "UV");
    glBindAttribLocation(_glHandles.shaderHandle, (GLuint)Attribute::VertexColor,  "Color");
    glLinkProgram(_glHandles.shaderHandle);
    gl_checkProgram(_glHandles.shaderHandle, "shader program", glslVersionString);

    _glHandles.textureUniformLocation = glGetUniformLocation(_glHandles.shaderHandle, "Texture");
    checkGLError();
}

void GLShader::enable (int32_t textureId)
{
    dl_assert (_glHandles.shaderHandle != 0, "Forgot to call initialize()?");
    glGetIntegerv(GL_CURRENT_PROGRAM, &impl->prevHandle);
    glUseProgram (_glHandles.shaderHandle);
    glUniform1i (_glHandles.textureUniformLocation, textureId);
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

GLRestoreStateAfterScope_Texture::GLRestoreStateAfterScope_Texture()
{
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &_prevTexture);
}

GLRestoreStateAfterScope_Texture::~GLRestoreStateAfterScope_Texture()
{
    glBindTexture (GL_TEXTURE_2D, _prevTexture);
}

GLTexture::~GLTexture()
{
    releaseGL();
}

void GLTexture::initializeWithExistingTextureID(uint32_t textureId, int width, int height)
{
    if (_textureId != 0)
        releaseGL();

    _textureId = textureId;
    _width = width;
    _height = height;

    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

void GLTexture::releaseGL()
{
    if (_textureId != 0)
    {
        glDeleteTextures(1, &_textureId);
        _textureId = 0;
    }
}

void GLTexture::initialize()
{
    GLRestoreStateAfterScope_Texture _;

    glGenTextures(1, &_textureId);
    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void GLTexture::ensureAllocatedForRGBA (int width, int height)
{
    GLRestoreStateAfterScope_Texture _;
        
    glBindTexture(GL_TEXTURE_2D, _textureId);
    _width = width;
    _height = height;
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
}

void GLTexture::upload(const dl::ImageSRGBA& im)
{
    GLRestoreStateAfterScope_Texture _;

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(im.bytesPerRow() / im.bytesPerPixel()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im.width(), im.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    _width = im.width();
    _height = im.height();
}

void GLTexture::uploadRgba(const uint8_t* rgbaBuffer, int width, int height, int bytesPerRow)
{
    GLRestoreStateAfterScope_Texture _;

    if (bytesPerRow <= 0)
        bytesPerRow = width*4;

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)(bytesPerRow / 4));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaBuffer);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    _width = width;
    _height = height;
}

void GLTexture::download (dl::ImageSRGBA& im)
{
    GLRestoreStateAfterScope_Texture _;
    im.ensureAllocatedBufferForSize (_width, _height);

    glBindTexture(GL_TEXTURE_2D, _textureId);
    glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)(im.bytesPerRow() / im.bytesPerPixel()));
    glGetTexImage (GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);    
#if PLATFORM_MACOS
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif

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

// --------------------------------------------------------------------------------
// GLFrameBuffer
// --------------------------------------------------------------------------------

namespace dl 
{

struct GLFrameBuffer::Impl
{
    bool fboInitialized = false;
    GLuint fbo = 0;
    // GLuint rbo = 0;
    GLuint rbo_depth = 0;
    GLint fboBeforeEnabled = 0;

    GLTexture outputColorTexture;
};

GLFrameBuffer::GLFrameBuffer ()
: impl (new Impl())
{}

GLFrameBuffer::~GLFrameBuffer ()
{
    if (impl->fboInitialized)
    {
        glDeleteFramebuffers(1, &impl->fbo);
        // glDeleteRenderbuffers(1, &impl->rbo);
        glDeleteRenderbuffers(1, &impl->rbo_depth);
    }
}

GLTexture& GLFrameBuffer::outputColorTexture ()
{
    return impl->outputColorTexture;
}

void GLFrameBuffer::enable(int width, int height)
{
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &impl->fboBeforeEnabled);

    if (!impl->fboInitialized)
    {
        glGenFramebuffers(1, &impl->fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, impl->fbo);
        
        glGenRenderbuffers(1, &impl->rbo_depth);
        // glGenRenderbuffers(1, &impl->rbo);
        impl->outputColorTexture.initialize ();

        impl->fboInitialized = true;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, impl->fbo);     

    if (impl->outputColorTexture.width() != width || impl->outputColorTexture.height() != height)
    {
        // We should not need a RBO for the color attachment anymore, the texture should be enough.
        // glBindRenderbuffer(GL_RENDERBUFFER, impl->rbo);
        // glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, impl->rbo);

        impl->outputColorTexture.ensureAllocatedForRGBA (width, height);
        glBindTexture(GL_TEXTURE_2D, impl->outputColorTexture.textureId());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, impl->outputColorTexture.textureId(), 0);

        glBindRenderbuffer(GL_RENDERBUFFER, impl->rbo_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, impl->rbo_depth);
        
        dl_assert (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FB creation failed.");
    }

    glViewport(0,0,width,height);
}

void GLFrameBuffer::disable()
{
    glBindFramebuffer(GL_FRAMEBUFFER, impl->fboBeforeEnabled); 
    impl->fboBeforeEnabled = 0;
}

void GLFrameBuffer::downloadBuffer(ImageSRGBA& output) const
{
    output.ensureAllocatedBufferForSize (impl->outputColorTexture.width(), impl->outputColorTexture.height());
    glPixelStorei (GL_UNPACK_ROW_LENGTH, GLint(output.bytesPerRow() / output.bytesPerPixel()));
    glReadPixels(0, 0,
                 impl->outputColorTexture.width(), impl->outputColorTexture.height(),
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 output.data());
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
}

} // dl

// --------------------------------------------------------------------------------
// ImageRenderer
// --------------------------------------------------------------------------------

namespace dl 
{

struct DrawVert
{
    vec2f pos;
    vec2f uv;
};

GLImageRenderer::~GLImageRenderer ()
{
    if (_vbo)
    {
        glDeleteBuffers (1, &_vbo);
        _vbo = 0;
    }

    if (_elementbuffer)
    {
        glDeleteBuffers (1, &_elementbuffer);
        _elementbuffer = 0;
    }

    if (_vao)
    {
        glDeleteVertexArrays (1, &_vao);
        _vao = 0;
    }
}

void GLImageRenderer::initializeGL ()
{
    glGenBuffers (1, &_vbo);
    glGenVertexArrays (1, &_vao);

    // OpenGL NDC goes from -1 to 1 so this will cover the full view.
    // UV coordinates bottomLeft is at (0,0), topRight at (1,1)
    static std::array<DrawVert, 4> vertices = {
        DrawVert { {-1,-1}, {0,0} }, // bottom left
        DrawVert { {-1, 1}, {0,1} }, // top left
        DrawVert { { 1, 1}, {1,1} }, // top right
        DrawVert { { 1,-1}, {1,0} }, // bottom right
    };

    // Setup the layout.
    glBindBuffer (GL_ARRAY_BUFFER, _vbo);
    glBufferData (GL_ARRAY_BUFFER, vertices.size () * sizeof (DrawVert), (const GLvoid*)vertices.data (), GL_STATIC_DRAW);

    glBindVertexArray (_vao);
    glEnableVertexAttribArray ((GLuint)GLShader::Attribute::VertexPos);
    glEnableVertexAttribArray ((GLuint)GLShader::Attribute::VertexUV);
    glVertexAttribPointer ((GLuint)GLShader::Attribute::VertexPos, 2, GL_FLOAT, GL_FALSE, sizeof (DrawVert), (GLvoid*)offsetof (DrawVert, pos));
    glVertexAttribPointer ((GLuint)GLShader::Attribute::VertexUV, 2, GL_FLOAT, GL_FALSE, sizeof (DrawVert), (GLvoid*)offsetof (DrawVert, uv));

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    GLubyte indices[] = { 0,1,2, // first triangle (bottom left - top left - top right)
                          0,2,3 }; // second triangle (bottom left - top right - bottom right)
    glGenBuffers (1, &_elementbuffer);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, _elementbuffer);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof (GLubyte), indices, GL_STATIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

    checkGLError ();
}

void GLImageRenderer::render ()
{
    glBindVertexArray (_vao);
    glBindBuffer (GL_ARRAY_BUFFER, _vbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, _elementbuffer);

    glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0); // uses the GL_ELEMENT_ARRAY_BUFFER

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray (0);
}

} // dl