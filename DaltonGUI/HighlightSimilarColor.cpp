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

#include <DaltonGUI/ImguiUtils.h>
#include <DaltonGUI/ImguiGLFWWindow.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_internal.h"

// Note: need to include that before GLFW3 for some reason.
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

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
    dl::PixelSRGBA srgba = (*_im)(_selectedPixel.col, _selectedPixel.row);
    mutableData.shaderParams.activeColorSRGB01.x = srgba.r / 255.f;
    mutableData.shaderParams.activeColorSRGB01.y = srgba.g / 255.f;
    mutableData.shaderParams.activeColorSRGB01.z = srgba.b / 255.f;

    mutableData.activeColorHSV_1_1_255 = dl::convertToHSV(srgba);

    mutableData.shaderParams.hasActiveColor = true;
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

#if 0
        const auto filledRectSize = ImVec2(9*monoFontSize, 9*monoFontSize);
        ImVec2 topLeft = ImGui::GetCursorPos();
        ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
        ImVec2 bottomRight = topLeft + filledRectSize;

        auto *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32(sRgb.r, sRgb.g, sRgb.b, 255));
        drawList->AddRect(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32_WHITE);
        // Show a cross.
        if (!data.shaderParams.hasActiveColor)
        {
            ImVec2 imageTopLeft = topLeft + screenFromWindow;
            ImVec2 imageBottomRight = bottomRight + screenFromWindow - ImVec2(1, 1);
            drawList->AddLine(imageTopLeft, imageBottomRight, IM_COL32_WHITE);
            drawList->AddLine(ImVec2(imageTopLeft.x, imageBottomRight.y), ImVec2(imageBottomRight.x, imageTopLeft.y), IM_COL32_WHITE);
        }

        ImGui::SetCursorPosX(bottomRight.x + padding);
        ImGui::SetCursorPosY(topLeft.y);

        // Show the side info about the current color
        {
            // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1,0,0,1));
            ImGui::BeginChild("ColorInfo", ImVec2(monoFontSize*14, filledRectSize.y));

            if (data.shaderParams.hasActiveColor)
            {
                ImguiGLFWWindow::PushMonoSpaceFont(io);
                auto closestColors = dl::closestColorEntries(sRgb, dl::ColorDistance::CIE2000);

                ImGui::Text("%s / %s",
                            closestColors[0].entry->className,
                            closestColors[0].entry->colorName);

                ImGui::Text("sRGB: [%d %d %d]", sRgb.r, sRgb.g, sRgb.b);

                PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
                ImGui::Text("Linear RGB: [%d %d %d]", int(lrgb.r * 255.0), int(lrgb.g * 255.0), int(lrgb.b * 255.0));

                auto hsv = data.activeColorHSV_1_1_255;
                ImGui::Text("HSV: [%.1fº %.1f%% %.1f]", hsv.x * 360.f, hsv.y * 100.f, hsv.z);

                PixelLab lab = dl::convertToLab(sRgb);
                ImGui::Text("L*a*b: [%.1f %.1f %.1f]", lab.l, lab.a, lab.b);

                PixelXYZ xyz = convertToXYZ(sRgb);
                ImGui::Text("XYZ: [%.1f %.1f %.1f]", xyz.x, xyz.y, xyz.z);
                ImGui::PopFont();
            }
            else
            {
                ImGui::BulletText("Click on the image to\nhighlight pixels with\na similar color.");
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::BulletText("Up/Down or w/s to change\nthe filter.");
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::BulletText("'c' to copy the color\ninfo to the clipboard.");
            }

            ImGui::EndChild();
            // ImGui::PopStyleColor();
        }

        ImGui::SetCursorPosY(bottomRight.y + padding);
#endif
        
        // ImGui::Text("Tip: try the mouse wheel to adjust the threshold.");

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
