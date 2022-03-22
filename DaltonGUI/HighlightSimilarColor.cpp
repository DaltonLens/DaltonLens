//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#define GL_SILENCE_DEPRECATION 1
#define NOMINMAX

#include "HighlightSimilarColor.h"

#include <Dalton/OpenGL.h>
#include <Dalton/Platform.h>
#include <Dalton/Image.h>
#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/ColorConversion.h>
#include <Dalton/DeepAlias.h>

#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/ImguiGLFWWindow.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_internal.h"

// Note: need to include that before GLFW3 for some reason.
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include <zv/Client.h>

namespace dl
{

void HighlightRegionState::updateFrameCount ()
{
    mutableData.shaderParams.frameCount = ImGui::GetFrameCount();
}

void HighlightRegionState::clearSelection()
{
    mutableData.shaderParams.hasActiveColor = false;
    mutableData.shaderParams.activeColorSRGB01 = vec4d(0, 0, 0, 1);
    _selectedPixel = dl::vec2i(-1, -1);
    mutableData.cursorOverlayInfo = {};
}

void HighlightRegionState::ensureAliasedImageReady ()
{
    if (!mutableData.smartAlias)
        return;

    if (_imItem->aliasedIm.hasData())
        return;

    DeepAlias alias;
    _imItem->aliasedIm = alias.undoAntiAliasing (_imItem->im);

    zv::logImageRGBA ("input", _imItem->im.rawBytes (), _imItem->im.width (), _imItem->im.height (), _imItem->im.bytesPerRow ());
    zv::logImageRGBA ("aliased", _imItem->aliasedIm.rawBytes (), _imItem->aliasedIm.width (), _imItem->aliasedIm.height (), _imItem->aliasedIm.bytesPerRow ());

    _imItem->gpuAliasedTexture.upload (_imItem->aliasedIm);
}

void HighlightRegionState::setImage (ImageItem* imItem)
{ 
    _imItem = imItem;
    
    ensureAliasedImageReady ();
}

void HighlightRegionState::setSelectedPixel(float x, float y, const CursorOverlayInfo& cursorOverlayInfo)
{
    const auto newPixel = dl::vec2i((int)x, (int)y);

    if (mutableData.shaderParams.hasActiveColor)
    {
        // Toggle it.
        clearSelection();
        return;
    }

    _selectedPixel = newPixel;
    mutableData.shaderParams.hasActiveColor = true;
    updateSelectedPixelValue ();

    mutableData.cursorOverlayInfo = cursorOverlayInfo;
    updateDeltas();
}

void HighlightRegionState::addSliderDelta(float delta)
{
    mutableData.deltaColorThreshold -= delta;
    mutableData.deltaColorThreshold = dl::keepInRange(mutableData.deltaColorThreshold, 1.f, 20.f);
    updateDeltas();
}

void HighlightRegionState::togglePlotMode()
{
    mutableData.plotMode = !mutableData.plotMode;
    updateDeltas();
}

void HighlightRegionState::updateSelectedPixelValue ()
{
    if (!mutableData.shaderParams.hasActiveColor)
        return;

    dl::PixelSRGBA srgba = refIm()(_selectedPixel.col, _selectedPixel.row);
    mutableData.shaderParams.activeColorSRGB01.x = srgba.r / 255.f;
    mutableData.shaderParams.activeColorSRGB01.y = srgba.g / 255.f;
    mutableData.shaderParams.activeColorSRGB01.z = srgba.b / 255.f;

    mutableData.activeColorHSV_1_1_255 = dl::convertToHSV(srgba);
}

void HighlightRegionState::updateSmartAlias()
{
    ensureAliasedImageReady ();
    updateSelectedPixelValue ();
}

void HighlightRegionState::updateDeltas()
{
    // If the selected color is not saturated at all (grayscale), then we need rely on
    // value to discriminate the treat it the same way we treat the non-plot style.
    if (mutableData.plotMode && mutableData.activeColorHSV_1_1_255.y > 0.1)
    {        
        mutableData.shaderParams.deltaH_360 = mutableData.deltaColorThreshold;

        // The plot-mode has a much higher tolerance on value and saturation
        // because anti-aliased lines are blended with the background, which is
        // typically black/white/gray and will desaturate the line color after
        // alpha blending.
        // If the selected color is already not very saturated (like on aliased
        // an edge), then don't tolerate a huge delta.
        mutableData.shaderParams.deltaS_100 = mutableData.deltaColorThreshold * 5.f; // tolerates 3x more since the range is [0,100]
        mutableData.shaderParams.deltaS_100 *= mutableData.activeColorHSV_1_1_255.y;
        mutableData.shaderParams.deltaS_100 = std::max(mutableData.shaderParams.deltaS_100, 1.0f);
        mutableData.shaderParams.deltaV_255 = mutableData.deltaColorThreshold * 12.f; // tolerates much more difference than hue.
    }
    else
    {
        mutableData.shaderParams.deltaH_360 = mutableData.deltaColorThreshold;
        mutableData.shaderParams.deltaS_100 = mutableData.deltaColorThreshold; // tolerates 3x more since the range is [0,100]
        mutableData.shaderParams.deltaV_255 = mutableData.deltaColorThreshold; // tolerates slighly more since the range is [0,255]
    }
}

void HighlightRegionState::handleInputEvents ()
{
    auto& io = ImGui::GetIO();
    if (io.MouseWheel != 0.f)
    {
#if PLATFORM_MACOS
        const float scaleFactor = 5.f;
#else
        const float scaleFactor = 1.f;
#endif
        addSliderDelta (io.MouseWheel * scaleFactor);
    }

    if (ImGui::IsKeyPressed(GLFW_KEY_SPACE))
    {
        togglePlotMode();
    }
}

void renderHighlightRegionControls(HighlightRegionState &state, bool collapsed)
{
    auto& io = ImGui::GetIO();
    const float monoFontSize = ImguiGLFWWindow::monoFontSize(io);
    const float padding = monoFontSize / 2.f;

    // ImGuiWindowFlags flags = (/*ImGuiWindowFlags_NoTitleBar*/
    //                             // ImGuiWindowFlags_NoResize
    //                             // ImGuiWindowFlags_NoMove
    //                             // | ImGuiWindowFlags_NoScrollbar
    //                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize
    //                             // | ImGuiWindowFlags_NoFocusOnAppearing
    //                             // | ImGuiWindowFlags_NoBringToFrontOnFocus
    //                             | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNav);

    // ImGui::SetNextWindowCollapsed(collapsed);

    // Shortcut for the mutable data we can brutally update.
    auto &data = state.mutableData;

    // if (ImGui::Begin("DaltonLens - Selected color to Highlight", nullptr, flags))
    {
        const auto sRgb = dl::PixelSRGBA((int)(255.f * data.shaderParams.activeColorSRGB01.x + 0.5f),
                                         (int)(255.f * data.shaderParams.activeColorSRGB01.y + 0.5f),
                                         (int)(255.f * data.shaderParams.activeColorSRGB01.z + 0.5f),
                                         255);
        
        state.handleInputEvents();

        int prevDeltaInt = int (data.deltaColorThreshold + 0.5f);
        int deltaInt = prevDeltaInt;
        ImGui::SliderInt ("Max Difference", &deltaInt, 1, 20);
        if (deltaInt != prevDeltaInt)
        {
            data.deltaColorThreshold = deltaInt;
            state.updateDeltas ();
        }

        if (ImGui::Checkbox("Plot Mode", &data.plotMode))
        {
            state.updateDeltas();
        }
        ImGui::SameLine();
        helpMarker("Allow more difference in saturation and value "
                   "to better handle anti-aliasing on lines and curves. "
                   "Better to disable it when looking at flat colors, "
                   "for example on pie charts.", monoFontSize*20);

        if (data.plotMode)
        {
            ImGui::SameLine ();
            if (ImGui::Checkbox ("Smart Aliasing", &data.smartAlias))
            {
                
            }
            ImGui::SameLine ();
            helpMarker ("[Experimental] use deep learning to remove "
                        "anti-aliasing on lines and edges.", monoFontSize * 20);
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        if (data.shaderParams.hasActiveColor)
        {
//            ImGui::Text("Hue  in [%.0fº -> %.0fº]",
//                        (data.activeColorHSV_1_1_255.x * 360.f) - data.shaderParams.deltaH_360,
//                        (data.activeColorHSV_1_1_255.x * 360.f) + data.shaderParams.deltaH_360);
//            ImGui::Text("Sat. in [%.0f%% -> %.0f%%]",
//                        (data.activeColorHSV_1_1_255.y * 100.f) - data.shaderParams.deltaS_100,
//                        (data.activeColorHSV_1_1_255.y * 100.f) + data.shaderParams.deltaS_100);
//            ImGui::Text("Val. in [%.0f -> %.0f]",
//                        (data.activeColorHSV_1_1_255.z) - data.shaderParams.deltaV_255,
//                        (data.activeColorHSV_1_1_255.z) + data.shaderParams.deltaV_255);
            
            ImGui::Text("Selected HSV Range:");
            ImguiGLFWWindow::PushMonoSpaceFont(io);

            float hueMin = (data.activeColorHSV_1_1_255.x * 360.f) - data.shaderParams.deltaH_360;
            if (hueMin < 0) hueMin = 360.f - hueMin;
            float hueMax = fmod((data.activeColorHSV_1_1_255.x * 360.f) + data.shaderParams.deltaH_360, 360.f);

            float satMin = std::max(0.f, (data.activeColorHSV_1_1_255.y * 100.f) - data.shaderParams.deltaS_100);
            float satMax = std::min (100.f, (data.activeColorHSV_1_1_255.y * 100.f) + data.shaderParams.deltaS_100);

            float valueMin = std::max(0.f, (data.activeColorHSV_1_1_255.z) - data.shaderParams.deltaV_255);
            float valueMax = std::min(255.f, (data.activeColorHSV_1_1_255.z) + data.shaderParams.deltaV_255);

            ImGui::Text("H=[%.0f %.0f]º S=[%.0f %.0f]%% V=[%.0f %.0f]%%",
                        hueMin, hueMax,
                        satMin, satMax,
                        valueMin*100.f/255.f, valueMax*100.f/255.f);
            ImGui::PopFont();
        }
        else
        {
            ImGui::Text("No selected color. Click on the image to pick one.");
            ImguiGLFWWindow::PushMonoSpaceFont(io);
            ImGui::TextDisabled("H=[N/A]º S=[N/A]%% V=[N/A]");
            ImGui::PopFont();
        }
        
    }
    // ImGui::End();
}

} // dl
