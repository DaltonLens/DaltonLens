//
//  Graphics.cpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#include "Graphics.h"

#include <Dalton/Utils.h>

#include <GL/gl3w.h>

#include <imgui.h>

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

GLTexture::~GLTexture ()
{
    releaseGL ();
}

void GLTexture::initializeWithExistingTextureID (uint32_t textureId)
{
    if (_textureId != 0)
        releaseGL();
    
    _textureId = textureId;
}

void GLTexture::releaseGL ()
{
    glDeleteTextures(1, &_textureId);
    _textureId = 0;
}

void GLTexture::initialize ()
{
    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
    
    glGenTextures(1, &_textureId);
    glBindTexture(GL_TEXTURE_2D, _textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

void GLTexture::upload (const dl::ImageSRGBA& im)
{
    GLint prevTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
    
    glBindTexture(GL_TEXTURE_2D, _textureId);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerRow()/im.bytesPerPixel());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im.width(), im.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rawBytes());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    
    glBindTexture(GL_TEXTURE_2D, prevTexture);
}

} // dl

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
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetProgramInfoLog(handle, log_length, NULL, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
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
        ImVector<char> buf;
        buf.resize((int)(log_length + 1));
        glGetShaderInfoLog(handle, log_length, NULL, (GLchar *)buf.begin());
        fprintf(stderr, "%s\n", buf.begin());
    }
    return (GLboolean)status == GL_TRUE;
}

struct GLShader::Impl
{
    Impl(GLShader& that)
    : that(that)
    {}
    
    GLShader& that;
    
    GLuint shaderHandle = 0;
    GLuint vertHandle = 0;
    GLuint fragHandle = 0;
    GLuint attribLocationTex = 0;
    GLuint attribLocationProjMtx = 0;
    GLuint attribLocationVtxPos = 0;
    GLuint attribLocationVtxUV = 0;
    GLuint attribLocationVtxColor = 0;

    GLint prevShaderHandle = 0;

    ImGuiViewport* viewportWhenEnabled = nullptr;
    
    std::function<void(GLShader&)> extraRenderCallback = nullptr;
    
    std::string name = "Unknown";
    
    void onRenderEnable (const ImDrawList *parent_list, const ImDrawCmd *drawCmd)
    {
        auto* drawData = viewportWhenEnabled->DrawData;
        float L = drawData->DisplayPos.x;
        float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
        float T = drawData->DisplayPos.y;
        float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

        const float ortho_projection[4][4] =
        {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };

        glGetIntegerv(GL_CURRENT_PROGRAM, &prevShaderHandle);
        glUseProgram (shaderHandle);

        glUniformMatrix4fv(attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
        glUniform1i(attribLocationTex, 0);
        
        if (extraRenderCallback)
            extraRenderCallback(that);
        
        checkGLError();
    }

    void onRenderDisable (const ImDrawList *parent_list, const ImDrawCmd *drawCmd)
    {
        glUseProgram (prevShaderHandle);
        prevShaderHandle = 0;
        viewportWhenEnabled = nullptr;
    }
};

static const GLchar* fragmentLibrary() {
    return R"(
         const float hsxEpsilon = 1e-10;
    
         vec3 yCbCrFromSRGBA (vec4 srgba)
         {
             // FIXME: this is ignoring gamma and treating it like linearRGB
             float y =   0.57735027*srgba.r + 0.57735027*srgba.g + 0.57735027*srgba.b;
             float cr =  0.70710678*srgba.r - 0.70710678*srgba.g;
             float cb = -0.40824829*srgba.r - 0.40824829*srgba.g + 0.81649658*srgba.b;
             return vec3(y,cb,cr);
         }

         vec4 sRGBAfromYCbCr (vec3 yCbCr, float alpha)
         {
             // FIXME: this is ignoring gamma and treating it like linearRGB
             
             vec4 srgbaOut;
             
             float y = yCbCr.x;
             float cb = yCbCr.y;
             float cr = yCbCr.z;
             
             srgbaOut.r = clamp(5.77350269e-01*y + 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
             srgbaOut.g = clamp(5.77350269e-01*y - 7.07106781e-01*cr - 4.08248290e-01*cb, 0.0, 1.0);
             srgbaOut.b = clamp(5.77350269e-01*y +            0.0*cr + 8.16496581e-01*cb, 0.0, 1.0);
             srgbaOut.a = alpha;
             return srgbaOut;
         }
    
         vec3 lmsFromSRGBA (vec4 srgba)
         {
             //    17.8824, 43.5161, 4.11935,
             //    3.45565, 27.1554, 3.86714,
             //    0.0299566, 0.184309, 1.46709
         
             vec3 lms;
             lms[0] = 17.8824*srgba.r + 43.5161*srgba.g + 4.11935*srgba.b;
             lms[1] = 3.45565*srgba.r + 27.1554*srgba.g + 3.86714*srgba.b;
             lms[2] = 0.0299566*srgba.r + 0.184309*srgba.g + 1.46709*srgba.b;
             return lms;
         }

         vec4 sRGBAFromLms (vec3 lms, float alpha)
         {
             //    0.0809445    -0.130504     0.116721
             //    -0.0102485    0.0540193    -0.113615
             //    -0.000365297  -0.00412162     0.693511
         
             vec4 srgbaOut;
             srgbaOut.r = clamp(0.0809445*lms[0] - 0.130504*lms[1] + 0.116721*lms[2], 0.0, 1.0);
             srgbaOut.g = clamp(-0.0102485*lms[0] + 0.0540193*lms[1] - 0.113615*lms[2], 0.0, 1.0);
             srgbaOut.b = clamp(-0.000365297*lms[0] - 0.00412162*lms[1] + 0.693511*lms[2], 0.0, 1.0);
             srgbaOut.a = alpha;
             return srgbaOut;
         }
    
         // H in [0,1]
         // C in [0,1]
         // V in [0,1]
         vec3 HCVFromRGB(vec3 RGB)
         {
             // Based on work by Sam Hocevar and Emil Persson
             vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0/3.0) : vec4(RGB.gb, 0.0, -1.0/3.0);
             vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
             float C = Q.x - min(Q.w, Q.y);
             float H = abs((Q.w - Q.y) / (6 * C + hsxEpsilon) + Q.z);
             return vec3(H, C, Q.x);
         }
         
         // H in [0,1]
         // C in [0,1]
         // V in [0,1]
         vec3 HSVFromSRGB (vec3 srgb)
         {
             // FIXME: this is ignoring gamma and treating it like linearRGB
         
             vec3 HCV = HCVFromRGB(srgb.xyz);
             float S = HCV.y / (HCV.z + hsxEpsilon);
             return vec3(HCV.x, S, HCV.z);
         }
     
         vec3 RGBFromHUE(float H)
         {
             float R = abs(H * 6 - 3) - 1;
             float G = 2 - abs(H * 6 - 2);
             float B = 2 - abs(H * 6 - 4);
             return clamp(vec3(R,G,B), 0, 1);
         }
    
         vec4 sRGBAFromHSV(vec3 HSV, float alpha)
         {
             vec3 RGB = RGBFromHUE(HSV.x);
             return vec4(((RGB - 1) * HSV.y + 1) * HSV.z, alpha);
         }
    
         vec3 applyProtanope (vec3 lms)
         {
             //    p.l = /* 0*p.l + */ 2.02344*p.m - 2.52581*p.s;
             vec3 lmsTransformed = lms;
             lmsTransformed[0] = 2.02344*lms[1] - 2.52581*lms[2];
             return lmsTransformed;
         }

         vec3 applyDeuteranope (vec3 lms)
         {
             //    p.m = 0.494207*p.l /* + 0*p.m */ + 1.24827*p.s;
             vec3 lmsTransformed = lms;
             lmsTransformed[1] = 0.494207*lms[0] + 1.24827*lms[2];
             return lmsTransformed;
         }

         vec3 applyTritanope (vec3 lms)
         {
             //    p.s = -0.395913*p.l + 0.801109*p.m /* + 0*p.s */;
             vec3 lmsTransformed = lms;
             lmsTransformed[2] = -0.395913*lms[0] + 0.801109*lms[1];
             return lmsTransformed;
         }
    
         vec4 daltonizeV1 (vec4 srgba, vec4 srgbaSimulated)
         {
             vec3 error = srgba.rgb - srgbaSimulated.rgb;
        
             float updatedG = srgba.g + 0.7*error.r + 1.0*error.g + 0.0*error.b;
             float updatedB = srgba.b + 0.7*error.r + 0.0*error.g + 1.0*error.b;
        
             vec4 srgbaOut = srgba;
             srgbaOut.g = clamp(updatedG, 0.0, 1.0);
             srgbaOut.b = clamp(updatedB, 0.0, 1.0);
             return srgbaOut;
         }
    )";
}
    
GLShader::GLShader ()
: impl (new Impl(*this))
{
    
}

GLShader::~GLShader() = default;

const std::string& GLShader::name() const { return impl->name; }

void GLShader::initialize(const std::string& name, const char* glslVersionString, const GLchar* vertexShader, const GLchar* fragmentShader)
{
    impl->name = name;
    
    const GLchar *defaultVertexShader_glsl_130 =
    "uniform mat4 ProjMtx;\n"
    "in vec2 Position;\n"
    "in vec2 UV;\n"
    "in vec4 Color;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";
    
    const GLchar *defaultFragmentShader_glsl_130 =
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "out vec4 Out_Color;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";
    
    // Select shaders matching our GLSL versions
    if (vertexShader == nullptr)
        vertexShader = defaultVertexShader_glsl_130;
    
    if (fragmentShader == nullptr)
        fragmentShader = defaultFragmentShader_glsl_130;
    
    // Create shaders
    const GLchar *vertex_shader_with_version[3] = {glslVersionString, "\n", vertexShader};
    impl->vertHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(impl->vertHandle, 3, vertex_shader_with_version, NULL);
    glCompileShader(impl->vertHandle);
    gl_checkShader(impl->vertHandle, "vertex shader");
    
    const GLchar *fragment_shader_with_version[4] = {glslVersionString, "\n", fragmentLibrary(), fragmentShader};
    impl->fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(impl->fragHandle, 4, fragment_shader_with_version, NULL);
    glCompileShader(impl->fragHandle);
    gl_checkShader(impl->fragHandle, "fragment shader");
    
    impl->shaderHandle = glCreateProgram();
    glAttachShader(impl->shaderHandle, impl->vertHandle);
    glAttachShader(impl->shaderHandle, impl->fragHandle);
    glLinkProgram(impl->shaderHandle);
    gl_checkProgram(impl->shaderHandle, "shader program", glslVersionString);
    
    impl->attribLocationTex = glGetUniformLocation(impl->shaderHandle, "Texture");
    impl->attribLocationProjMtx = glGetUniformLocation(impl->shaderHandle, "ProjMtx");
    impl->attribLocationVtxPos = (GLuint)glGetAttribLocation(impl->shaderHandle, "Position");
    impl->attribLocationVtxUV = (GLuint)glGetAttribLocation(impl->shaderHandle, "UV");
    impl->attribLocationVtxColor = (GLuint)glGetAttribLocation(impl->shaderHandle, "Color");
    
    checkGLError();
}
    
uint32_t GLShader::shaderHandle() const { return impl->shaderHandle; }

void GLShader::setExtraUserCallback (const std::function<void(GLShader&)>& extraRenderCallback)
{
    impl->extraRenderCallback = extraRenderCallback;
}

void GLShader::enable ()
{
    // We need to store the viewport to check its display size later on.
    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    dl_assert (impl->viewportWhenEnabled == nullptr || viewport == impl->viewportWhenEnabled,
               "You can't enable it multiple times for windows that are not in the same viewport");
    dl_assert (viewport != nullptr, "Invalid viewport.");
    
    impl->viewportWhenEnabled = viewport;
    
    auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
    {
        GLShader* that = reinterpret_cast<GLShader*>(cmd->UserCallbackData);
        dl_assert (that, "Invalid user data");
        that->impl->onRenderEnable (parent_list, cmd);
    };
    
    ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
}

void GLShader::disable ()
{
    auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
    {
        GLShader* that = reinterpret_cast<GLShader*>(cmd->UserCallbackData);
        dl_assert (that, "Invalid user data");
        that->impl->onRenderDisable (parent_list, cmd);
    };
    
    ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
}

} // dl
