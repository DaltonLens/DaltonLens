//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/MathUtils.h>

#include <imgui.h>

namespace dl
{

inline ImVec2 imVec2 (dl::Point p) { return ImVec2(p.x, p.y); }
inline ImVec2 imPos (dl::Rect& r) { return imVec2(r.origin); }
inline ImVec2 imSize (dl::Rect& r) { return imVec2(r.size); }

}
