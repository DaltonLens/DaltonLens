//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/Image.h>
#include <Dalton/Filters.h>

#include <DaltonGUI/ImageItem.h>
#include <DaltonGUI/ImageCursorOverlay.h>

#include "imgui.h"

namespace dl
{

class HighlightRegionState
{
public:
    // Can access these guys directly, but may need to call updateDeltas after changing them.
    struct MutableData
    {
        Filter_HighlightSimilarColors::Params shaderParams;

        // H and S within [0,1]. V within [0,255]
        dl::PixelHSV activeColorHSV_1_1_255 = dl::PixelHSV(0, 0, 0);
        float deltaColorThreshold = 10.f;

        bool plotMode = true;
        bool smartAlias = false;

        CursorOverlayInfo cursorOverlayInfo;
    };
    MutableData mutableData;

public:
    void setImage(ImageItem* imItem);
    void clearSelection();
    void setSelectedPixel(float x, float y, const CursorOverlayInfo& cursorOverlayInfo);
    void addSliderDelta(float delta);
    void togglePlotMode();    
    void updateDeltas();
    bool hasActiveColor() const { return mutableData.shaderParams.hasActiveColor; }
    void updateFrameCount ();
    void handleInputEvents ();

private:
    void updateSelectedPixelValue ();

private:
    dl::vec2i _selectedPixel = dl::vec2i(0, 0);
    dl::ImageItem* _imItem = nullptr;
};

void renderHighlightRegionControls (HighlightRegionState& state, bool collapsed);

} // dl
