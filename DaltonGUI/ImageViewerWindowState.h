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
    Daltonize,
    HSVTransform,
    FlipRedBlue,
    FlipRedBlueInvertRed,

    NumModes,
};

std::string daltonViewerModeName (DaltonViewerMode mode);

bool viewerModeIsDaltonize (DaltonViewerMode mode);

struct ImageViewerWindowState
{
    HighlightRegionState highlightRegion;
    DaltonViewerMode activeMode = DaltonViewerMode::None;
    
    // modeForCurrentFrame can be different from activeMode
    // if the user presses the SHIFT key.
    DaltonViewerMode modeForCurrentFrame = DaltonViewerMode::None;

    Filter_Daltonize::Params daltonizeParams;

    Filter_HSVTransform::Params hsvTransform;

    struct InputState
    {
        bool shiftIsPressed = false;
    };

    InputState controlsInputState;
    InputState inputState;
};

} // dl