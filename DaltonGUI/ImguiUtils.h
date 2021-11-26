//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/MathUtils.h>
#include <IconsFontAwesome5.h>

#include <imgui.h>

namespace dl
{

inline ImVec2 imVec2 (dl::Point p) { return ImVec2(p.x, p.y); }
inline ImVec2 imPos (dl::Rect& r) { return imVec2(r.origin); }
inline ImVec2 imSize (dl::Rect& r) { return imVec2(r.size); }

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
inline void helpMarker(const char* desc, float wrapWidth)
{
    ImGui::Text(ICON_FA_QUESTION_CIRCLE);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(wrapWidth);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline ImGuiWindowFlags windowFlagsWithoutAnything()
{
    return (ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse
            | ImGuiWindowFlags_NoCollapse
            // | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_HorizontalScrollbar
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoNav);
}

}
