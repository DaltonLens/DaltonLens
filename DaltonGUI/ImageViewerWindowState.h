//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <DaltonGUI/HighlightSimilarColor.h>

namespace dl
{

enum class DaltonViewerMode {
    None = -2,
    Original = -1,

    HighlightRegions = 0,
    Protanope,
    Deuteranope,
    Tritanope,
    FlipRedBlue,
    FlipRedBlueInvertRed,

    NumModes,
};

std::string daltonViewerModeName (DaltonViewerMode mode);
struct ImageViewerWindowState
{
    HighlightRegionState highlightRegion;
    DaltonViewerMode currentMode = DaltonViewerMode::None;
};

} // dl