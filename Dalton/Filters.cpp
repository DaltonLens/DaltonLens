//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Filters.h"

#include <Dalton/OpenGL.h>
#include <Dalton/OpenGL_Shaders.h>
#include <Dalton/Utils.h>
#include <Dalton/ColorConversion.h>

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

GLShader* GLFilter::glShader () const
{
    return &impl->shader;
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
// GLFilterProcessor
// --------------------------------------------------------------------------------

namespace dl 
{

struct GLFilterProcessor::Impl
{
    GLFrameBuffer frameBuffer;
    GLImageRenderer renderer;
};

GLFilterProcessor::GLFilterProcessor()
: impl (new Impl())
{}

GLFilterProcessor::~GLFilterProcessor() = default;

void GLFilterProcessor::initializeGL ()
{
    impl->renderer.initializeGL();
}

void GLFilterProcessor::render (GLFilter& filter, uint32_t inputTextureId, int width, int height, ImageSRGBA* output)
{
    impl->frameBuffer.enable(width, height);
    glBindTexture(GL_TEXTURE_2D, inputTextureId);
    filter.enableGLShader ();
    impl->renderer.render ();
    filter.disableGLShader ();
    if (output)
    {
        impl->frameBuffer.downloadBuffer (*output);
    }
    impl->frameBuffer.disable();
}

GLTexture& GLFilterProcessor::filteredTexture()
{
    return impl->frameBuffer.outputColorTexture();
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

void applyDaltonizeSimulation (ImageLMS& lmsImage, Filter_Daltonize::Params::Kind blindness)
{
    // From daltonize.py:
    //        lms2lmsp = [0 2.02344 -2.52581; 0 1 0; 0 0 1] ;
    //        lms2lmsd = [1 0 0; 0.494207 0 1.24827; 0 0 1] ;
    //        lms2lmst = [1 0 0; 0 1 0; -0.395913 0.801109 0] ;
    
    auto protanope = [](int c, int r, PixelLMS& p) {
        p.l = /* 0*p.l + */ 2.02344*p.m - 2.52581*p.s;
    };
    
    auto deuteranope = [](int c, int r, PixelLMS& p) {
        p.m = 0.494207*p.l /* + 0*p.m */ + 1.24827*p.s;
    };
    
    auto tritanope = [](int c, int r, PixelLMS& p) {
        p.s = -0.395913*p.l + 0.801109*p.m /* + 0*p.s */;
    };
    
    switch (blindness)
    {
        case Filter_Daltonize::Params::Protanope:
            lmsImage.apply (protanope);
            break;
            
        case Filter_Daltonize::Params::Deuteranope:
            lmsImage.apply (deuteranope);
            break;
            
        case Filter_Daltonize::Params::Tritanope:
            lmsImage.apply (tritanope);
            break;
            
        default:
            assert (false);
    }
}

void Filter_Daltonize::applyCPU (const ImageSRGBA& input, ImageSRGBA& output) const
{
    SRGBAToLMSConverter converter;
    ImageLMS lmsImage;
    converter.convertToLms (input, lmsImage);
    applyDaltonizeSimulation (lmsImage, _currentParams.kind);

    ImageSRGBA simulatedSRGBA;
    converter.convertToSrgba (lmsImage, simulatedSRGBA);

    output = input;
    output.apply ([&](int c, int r, PixelSRGBA& srgba) {

        // [0, 0, 0],
        // [0.7, 1, 0],
        // [0.7, 0, 1]

        const auto& simRgba = simulatedSRGBA (c, r);

        float rError = srgba.r - simRgba.r;
        float gError = srgba.g - simRgba.g;
        float bError = srgba.b - simRgba.b;

        float updatedG = srgba.g + 0.7 * rError + 1.0 * gError + 0.0 * bError;
        float updatedB = srgba.b + 0.7 * rError + 0.0 * gError + 1.0 * bError;
        srgba.g = saturateAndCast (updatedG);
        srgba.b = saturateAndCast (updatedB);
    });
}

} // dl