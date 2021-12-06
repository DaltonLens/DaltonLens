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

namespace dl
{

void Filter_HSVTransform::initializeGL ()
{
    GLFilter::initializeGL (glslVersion(), nullptr, fragmentShader_HSVTransform_glsl_130); 

    GLuint shaderHandle = glHandles().shaderHandle;
    _attribLocationHueShift = (GLuint)glGetUniformLocation(shaderHandle, "u_hueShift");
    _attribLocationSaturationScale = (GLuint)glGetUniformLocation(shaderHandle, "u_saturationScale");
    _attribLocationHueQuantization = (GLuint)glGetUniformLocation(shaderHandle, "u_hueQuantization");
}

void Filter_HSVTransform::enableGLShader ()
{
    GLFilter::enableGLShader ();
    glUniform1f(_attribLocationHueShift, _currentParams.hueShift / 360.f);
    glUniform1f(_attribLocationSaturationScale, _currentParams.saturationScale);
    glUniform1f(_attribLocationHueQuantization, (float)_currentParams.hueQuantization);
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
    _attribLocationRefColor_sRGB = (GLuint)glGetUniformLocation(shaderHandle, "u_refColor_sRGB");
    _attribLocationDeltaH = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaH_360");
    _attribLocationDeltaS = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaS_100");
    _attribLocationDeltaV = (GLuint)glGetUniformLocation(shaderHandle, "u_deltaV_255");
    _attribLocationFrameCount = (GLuint)glGetUniformLocation(shaderHandle, "u_frameCount");
}

void Filter_HighlightSimilarColors::enableGLShader ()
{
    GLFilter::enableGLShader ();
    glUniform3f(_attribLocationRefColor_sRGB,
                _currentParams.activeColorSRGB01.x,
                _currentParams.activeColorSRGB01.y,
                _currentParams.activeColorSRGB01.z);
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
    _attribLocationSeverity = (GLuint)glGetUniformLocation(shaderHandle, "u_severity");    
}

void Filter_Daltonize::enableGLShader ()
{
    GLFilter::enableGLShader ();
    glUniform1i(_attribLocationKind, static_cast<int>(_currentParams.kind));
    glUniform1i(_attribLocationSimulateOnly, _currentParams.simulateOnly);
    glUniform1f(_attribLocationSeverity, _currentParams.severity);
}

void applyDaltonizeSimulation (ImageLMS& lmsImage, Filter_Daltonize::Params::Kind blindness, float severity)
{
    // See DaltonLens-Python and libDaltonLens to understand where the hardcoded
    // values come from.
    
    auto protanope = [severity](int c, int r, PixelLMS& p) {
        // Viénot 1999.
        p.l = (1.f-severity)*p.l + severity*(2.02344*p.m - 2.52580*p.s);
    };
    
    auto deuteranope = [severity](int c, int r, PixelLMS& p) {
        // Viénot 1999.
        p.m = (1.f-severity)*p.m + severity*(0.49421*p.l + + 1.24827*p.s);
    };
    
    auto tritanope = [severity](int c, int r, PixelLMS& p) {
        // Brettel 1997.
        // Check which plane.
        if ((p.l*0.34478 - p.m*0.65518) >= 0)
        {
            // Plane 1 for tritanopia
            p.s = (1.f-severity)*p.s + severity*(-0.00257*p.l + 0.05366*p.m);
        }
        else
        {
            // Plane 2 for tritanopia
            p.s = (1.f-severity)*p.s + severity*(-0.06011*p.l + 0.16299*p.m);
        }
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

void Filter_Daltonize::applyCPU (const ImageSRGBA& inputSRGBA, ImageSRGBA& output) const
{
    ImageLinearRGB inputRGB = convertToLinearRGB(inputSRGBA);

    RGBAToLMSConverter converter;
    ImageLMS lmsImage;
    converter.convertToLms (inputRGB, lmsImage);
    applyDaltonizeSimulation (lmsImage, _currentParams.kind, _currentParams.severity);

    ImageLinearRGB simulatedRGB;
    converter.convertToLinearRGB (lmsImage, simulatedRGB);

    if (_currentParams.simulateOnly)
    {
        output = convertToSRGBA(simulatedRGB);
        return;
    }

    ImageLinearRGB outputRGB = inputRGB;

    outputRGB.apply ([&](int c, int r, PixelLinearRGB& rgb) {

        // [0, 0, 0],
        // [0.7, 1, 0],
        // [0.7, 0, 1]

        const auto& simRgb = simulatedRGB (c, r);

        float rError = rgb.r - simRgb.r;
        float gError = rgb.g - simRgb.g;
        float bError = rgb.b - simRgb.b;

        float updatedG = rgb.g + 0.7 * rError + 1.0 * gError + 0.0 * bError;
        float updatedB = rgb.b + 0.7 * rError + 0.0 * gError + 1.0 * bError;
        rgb.g = updatedG;
        rgb.b = updatedB;
    });

    output = convertToSRGBA(outputRGB);
}

} // dl
