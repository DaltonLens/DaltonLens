//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#define GL_SILENCE_DEPRECATION 1

#include "ImguiOpenGL.h"

#include <Dalton/Utils.h>
#include <Dalton/Filters.h>

#include <GL/gl3w.h>

#include <imgui.h>

namespace dl
{

struct ImguiImageFilter::Impl
{
    GLFilter* activeFilter = nullptr;

    ImGuiViewport* viewportWhenEnabled = nullptr;
    
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

        activeFilter->enableGLShader ();

        const auto& handles = activeFilter->glHandles();

        glUniformMatrix4fv(handles.attribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
        glUniform1i(handles.attribLocationTex, 0);    

        // WARNING: here we're assuming that these attributes have the same location
        // as the ImGui default shader in the opengl3 backend, so it'll just work.
        // FIXME: This is dangerous though, we should probably re-assign them properly here.
        // unsigned attribLocationVtxPos = 0;
        // unsigned attribLocationVtxUV = 0;
        // unsigned attribLocationVtxColor = 0;
        
        checkGLError();
    }

    void onRenderDisable (const ImDrawList *parent_list, const ImDrawCmd *drawCmd)
    {
        activeFilter->disableGLShader ();        
        viewportWhenEnabled = nullptr;
        activeFilter = nullptr;
    }
};


    
ImguiImageFilter ::ImguiImageFilter  ()
: impl (new Impl())
{
    
}

ImguiImageFilter ::~ImguiImageFilter () = default;

void ImguiImageFilter ::enable (GLFilter* activeFilter)
{
    // We should not change the filter before rendering was fully completed, otherwise the callback
    // could get called on a different filter.
    dl_assert (impl->activeFilter == nullptr, "Still an active filter in flight, this is dangerous!");

    impl->activeFilter = activeFilter;

    // We need to store the viewport to check its display size later on.
    ImGuiViewport* viewport = ImGui::GetWindowViewport();
    dl_assert (impl->viewportWhenEnabled == nullptr || viewport == impl->viewportWhenEnabled,
               "You can't enable it multiple times for windows that are not in the same viewport");
    dl_assert (viewport != nullptr, "Invalid viewport.");
    
    impl->viewportWhenEnabled = viewport;
    
    auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
    {
        ImguiImageFilter * that = reinterpret_cast<ImguiImageFilter *>(cmd->UserCallbackData);
        dl_assert (that, "Invalid user data");
        that->impl->onRenderEnable (parent_list, cmd);
    };
    
    ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
}

void ImguiImageFilter ::disable ()
{
    auto shaderCallback = [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
    {
        ImguiImageFilter * that = reinterpret_cast<ImguiImageFilter *>(cmd->UserCallbackData);
        dl_assert (that, "Invalid user data");
        that->impl->onRenderDisable (parent_list, cmd);
    };
    
    ImGui::GetWindowDrawList()->AddCallback(shaderCallback, this);
}

} // dl
