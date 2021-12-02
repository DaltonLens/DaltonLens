//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "ImageCursorOverlay.h"

#include <DaltonGUI/PlatformSpecific.h>
#include <DaltonGUI/ImguiGLFWWindow.h>

#include <Dalton/Utils.h>
#include <Dalton/ColorConversion.h>

#include <GLFW/glfw3.h>
#include <clip/clip.h>

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace dl
{

void ImageCursorOverlay::showTooltip(const CursorOverlayInfo& d, bool showAsTooltip)
{
    if (!d.valid())
    {
        ImGui::Text("No color information under the mouse pointer.");
        return;
    }
    
    auto& io = ImGui::GetIO();
    const auto& image = *d.image;
    const auto& imageTexture = *d.imageTexture;
    
    const float monoFontSize = ImguiGLFWWindow::monoFontSize(io);
    const float padding = monoFontSize / 2.f;

    ImVec2 imageSize (image.width(), image.height());
    
    ImVec2 mousePosInImage (0,0);
    ImVec2 mousePosInTexture (0,0);
    {
        // This 0.5 offset is important since the mouse coordinate is an integer.
        // So when we are in the center of a pixel we'll return 0,0 instead of
        // 0.5,0.5.
        ImVec2 widgetPos = (d.mousePos + ImVec2(0.5f,0.5f)) - d.imageWidgetTopLeft;
        ImVec2 uv_window = widgetPos / d.imageWidgetSize;
        mousePosInTexture = (d.uvBottomRight-d.uvTopLeft)*uv_window + d.uvTopLeft;
        mousePosInImage = mousePosInTexture * imageSize;
    }
    
    if (!image.contains(mousePosInImage.x, mousePosInImage.y))
        return;
    
    {
        // When using a tooltip the window padding will change the initial offset of the
        // SameLine calls. This hack makes sure that it renders the same in the controls
        // window.
        float paddingIfNotTooltip = showAsTooltip ? 0.f : padding;
        
        if (showAsTooltip)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding,padding));
            ImGui::BeginTooltip();
        }
        
        const auto sRgb = image((int)mousePosInImage.x, (int)mousePosInImage.y);
        
        const int squareSize = 10*monoFontSize;

        // Show the zoomed image.
        const ImVec2 zoomItemSize (squareSize,squareSize);
        {
            const int zoomLenInPixels = int(d.roiWindowSize.x);
            ImVec2 pixelSizeInZoom = zoomItemSize / ImVec2(zoomLenInPixels,zoomLenInPixels);
            
            const ImVec2 zoomLen_uv (float(zoomLenInPixels) / image.width(), float(zoomLenInPixels) / image.height());
            ImVec2 zoom_uv0 = mousePosInTexture - zoomLen_uv*0.5f;
            ImVec2 zoom_uv1 = mousePosInTexture + zoomLen_uv*0.5f;
            
            ImVec2 zoomImageTopLeft = ImGui::GetCursorScreenPos();
            ImGui::Image(reinterpret_cast<ImTextureID>(imageTexture.textureId()), zoomItemSize, zoom_uv0, zoom_uv1);
            
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 p1 = pixelSizeInZoom * (zoomLenInPixels / 2) + zoomImageTopLeft;
            ImVec2 p2 = pixelSizeInZoom * ((zoomLenInPixels / 2) + 1) + zoomImageTopLeft;
            drawList->AddRect(p1, p2, IM_COL32(0,0,0,255));
            drawList->AddRect(p1 - ImVec2(1,1), p2 + ImVec2(1,1), IM_COL32(255,255,255,255));
        }
                
        ImGui::SameLine();
        
        float bottomOfSquares = NAN;

        // Show the central pixel color as a filled square
        {
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - ImGui::GetCursorPos();
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 bottomRight = topLeft + ImVec2(squareSize,squareSize);
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow, bottomRight + screenFromWindow, IM_COL32(sRgb.r, sRgb.g, sRgb.b, 255));
            ImGui::SetCursorPosX(topLeft.x + padding + monoFontSize*0.5f);
            ImGui::SetCursorPosY(topLeft.y + padding*3);
            bottomOfSquares = bottomRight.y;
        }
        
        // Show the help
        {
            ImguiGLFWWindow::PushMonoSpaceFont(io);

            ImVec4 color (1, 1, 1, 1);

            const auto hsv = dl::convertToHSV(sRgb);
            
            // This is a pretty expensive way to do it, but well, overall still very cheap.
            if (colorDistance_CIE2000(sRgb, PixelSRGBA(255,255,255,255)) < colorDistance_CIE2000(sRgb, PixelSRGBA(0,0,0,255)))
            {
                // White won't be visible on bright colors, so switch to a black
                // background if œthe contrast is higher.
                color = ImVec4(0.0, 0.0, 0.0, 1.0);
            }

            // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1,0,0,1));
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            ImGui::BeginChild("ColorInfo", ImVec2(squareSize - padding - monoFontSize*0.5f, zoomItemSize.y));
            
            ImGui::Text ("HTML #%02x%02x%02x", sRgb.r, sRgb.g, sRgb.b);
            
            ImGui::Text("sRGB %3d %3d %3d", sRgb.r, sRgb.g, sRgb.b);

            PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
            ImGui::Text(" RGB %3d %3d %3d", int(lrgb.r*255.0), int(lrgb.g*255.0), int(lrgb.b*255.0));
            
            PixelLab lab = dl::convertToLab(sRgb);
            ImGui::Text(" Lab %3d %3d %3d", intRnd(lab.l), intRnd(lab.a), intRnd(lab.b));
            
            PixelXYZ xyz = convertToXYZ(sRgb);
            ImGui::Text(" XYZ %3d %3d %3d", intRnd(xyz.x), intRnd(xyz.y), intRnd(xyz.z));
            
            ImGui::Text(" HSV %3d %3d %3d", intRnd(hsv.x*360.f), intRnd(hsv.y*100.f), intRnd(hsv.z*100.f/255.f));
                        
            ImGui::EndChild();
            ImGui::PopStyleColor();
            // ImGui::PopStyleColor();

            ImGui::PopFont();
        }
        
        auto closestColors = dl::closestColorEntries(sRgb, dl::ColorDistance::CIE2000);
        
        ImGui::SetCursorPosY (bottomOfSquares + padding);

        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(padding*2,padding*2);
            
            // Raw draw is in screen space.
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[0].entry->r, closestColors[0].entry->g, closestColors[0].entry->b, 255));
            
            ImGui::SetCursorPosX(bottomRight.x + padding);
        }

        auto addColorNameAndRGB = [&](const ColorEntry& entry, const int targetNameSize, float distance, bool disabled)
        {
            auto colorName = formatted("%s (%s", entry.className, entry.colorName);
            // auto colorName = formatted("%s / %s", entry.className, entry.colorName);
            if (colorName.size() >= targetNameSize)
            {
                colorName = colorName.substr(0, targetNameSize - 4) + "...)";
            }
            else if (colorName.size() < targetNameSize)
            {
                colorName += ')' + std::string(targetNameSize - colorName.size(), ' ');
            }
            
            if (disabled)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
            }
            ImGui::Text("%s", colorName.c_str());
            
            ImguiGLFWWindow::PushMonoSpaceFont(io, true /* small */);
            ImGui::SameLine(monoFontSize*11.2f - paddingIfNotTooltip);
            ImGui::Text("ΔE=%d", int(distance+0.5f));
            
            ImGui::SameLine(monoFontSize*14.f - paddingIfNotTooltip);
            
            const auto hsv = dl::convertToHSV(PixelSRGBA(entry.r, entry.g, entry.b, 255));
            
            ImGui::Text("HSV %3d %3d %3d",
                        intRnd(hsv.x*360.f),
                        intRnd(hsv.y*100.f),
                        intRnd(hsv.z*100.f/255.f));
            ImGui::PopFont();
            
            if (disabled)
            {
                ImGui::PopStyleColor();
            }
        };

        addColorNameAndRGB (*closestColors[0].entry, 21, closestColors[0].distance, false /* not disabled */);
        
        {
            ImVec2 topLeft = ImGui::GetCursorPos();
            ImVec2 screenFromWindow = ImGui::GetCursorScreenPos() - topLeft;
            ImVec2 bottomRight = topLeft + ImVec2(padding*2, padding*2);
            
            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(topLeft + screenFromWindow,
                                    bottomRight + screenFromWindow,
                                    IM_COL32(closestColors[1].entry->r, closestColors[1].entry->g, closestColors[1].entry->b, 255));
            ImGui::SetCursorPosX(bottomRight.x + padding);
        }

        const bool secondColorNotAsClose = (closestColors[1].distance - closestColors[0].distance) > 1.f;
        addColorNameAndRGB (*closestColors[1].entry, 21, closestColors[1].distance, secondColorNotAsClose);

        if (d.showHelp)
        {
            ImGui::Text ("drag to select a region | 'q': exit | 'c': copy");
        }
        
        if (!isnan(_timeOfLastCopyToClipboard))
        {
            if (currentDateInSeconds() - _timeOfLastCopyToClipboard < 1.0)
            {
                ImGui::Text ("Copied to clipboard.");
            }
            else
            {
                _timeOfLastCopyToClipboard = NAN;
            }
        }        

        if (showAsTooltip)
        {
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }

        if (ImGui::IsKeyPressed(GLFW_KEY_C))
        {
            std::string clipboardText;
            clipboardText += formatted("sRGB %d %d %d\n", sRgb.r, sRgb.g, sRgb.b);
            
            const PixelLinearRGB lrgb = dl::convertToLinearRGB(sRgb);
            clipboardText += formatted("linearRGB %.1f %.1f %.1f\n", lrgb.r, lrgb.g, lrgb.b);

            const auto hsv = dl::convertToHSV(sRgb);
            clipboardText += formatted("HSV %.1fº %.1f%% %.1f%%\n", hsv.x*360.f, hsv.y*100.f, hsv.z*100.f/255.f);

            PixelLab lab = dl::convertToLab(sRgb);
            clipboardText += formatted("L*a*b %.1f %.1f %.1f\n", lab.l, lab.a, lab.b);
            
            PixelXYZ xyz = convertToXYZ(sRgb);
            clipboardText += formatted("XYZ %.1f %.1f %.1f\n", xyz.x, xyz.y, xyz.z);

            // glfwSetClipboardString(nullptr, clipboardText.c_str());
            clip::set_text (clipboardText.c_str());
            _timeOfLastCopyToClipboard = currentDateInSeconds();
        }
    }
}

} // dl
