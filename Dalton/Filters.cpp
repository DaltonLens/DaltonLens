//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Filters.h"

#include <Dalton/OpenGL.h>
#include <Dalton/OpenGL_Shaders.h>
#include <Dalton/Utils.h>

#include <gl3w/GL/gl3w.h>
namespace dl
{

struct GLFilter::Impl
{
    GLShader shader;
};

GLFilter::GLFilter()
: impl (new Impl)
{
}
 
GLFilter::~GLFilter() = default;

void GLFilter::initializeGL(const char *glslVersionString, const char *vertexShader, const char *fragmentShader)
{
    impl->shader.initialize (glslVersionString, vertexShader, fragmentShader);
}

void GLFilter::enableGLShader()
{
    impl->shader.enable ();
}

void GLFilter::disableGLShader()
{
    impl->shader.disable ();
}

const GLShaderHandles& GLFilter::glHandles () const
{
    return impl->shader.glHandles();
}

void GLFilter::applyCPU (const ImageSRGBA& input, ImageSRGBA& output) const
{
    dl_assert (false, "unimplemented");
}

} // dl

// --------------------------------------------------------------------------------
// Simple filters
// --------------------------------------------------------------------------------
namespace dl
{

void Filter_FlipRedBlue::initializeGL ()
{ 
    GLFilter::initializeGL (glslVersion(), nullptr, fragmentShader_FlipRedBlue_glsl_130); 
}

void Filter_FlipRedBlueAndInvertRed::initializeGL ()
{ 
    GLFilter::initializeGL (glslVersion(), nullptr, fragmentShader_FlipRedBlue_InvertRed_glsl_130); 
}

} // dl

// --------------------------------------------------------------------------------
// Highlight Similar Colors
// --------------------------------------------------------------------------------
namespace dl
{

void Filter_HighlightSimilarColors::initializeGL ()
{
    GLFilter::initializeGL (glslVersion(), nullptr, fragmentShader_highlightSameColor); 
    GLuint shaderHandle = glHandles().shaderHandle;
    _attribLocationRefColor = (GLuint)glGetUniformLocation(shaderHandle, "u_refColor");
    _attribLocationDeltaH = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaH_360");
    _attribLocationDeltaS = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaS_100");
    _attribLocationDeltaV = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaV_255");
    _attribLocationFrameCount = (GLuint)glGetUniformLocation(shaderHandle, "u_frameCount");
}

void Filter_HighlightSimilarColors::enableGLShader ()
{
    GLFilter::enableGLShader ();
    glUniform3f(_attribLocationRefColor, _currentParams.activeColorRGB01.x, _currentParams.activeColorRGB01.y, _currentParams.activeColorRGB01.z);
    glUniform1f(_attribLocationDeltaH, _currentParams.deltaH_360);
    glUniform1f(_attribLocationDeltaS, _currentParams.deltaS_100);
    glUniform1f(_attribLocationDeltaV, _currentParams.deltaV_255);
    glUniform1i(_attribLocationFrameCount, _currentParams.frameCount / 2);
}

} // dl

// --------------------------------------------------------------------------------
// Daltonize
// --------------------------------------------------------------------------------

namespace dl 
{

void Filter_Daltonize::initializeGL ()
{
    GLFilter::initializeGL(glslVersion(), nullptr, fragmentShader_DaltonizeV1_glsl_130);
    GLuint shaderHandle = glHandles().shaderHandle;
    _attribLocationKind = (GLuint)glGetUniformLocation(shaderHandle, "u_kind");
    _attribLocationSimulateOnly = (GLuint)glGetUniformLocation(shaderHandle, "u_simulateOnly");
}

void Filter_Daltonize::enableGLShader ()
{
    GLFilter::enableGLShader ();
    glUniform1i(_attribLocationKind, static_cast<int>(_currentParams.kind));
    glUniform1i(_attribLocationSimulateOnly, _currentParams.simulateOnly);
}

} // dl