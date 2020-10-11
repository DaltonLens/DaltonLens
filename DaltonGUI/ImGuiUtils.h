//
//  ImGuiUtils.h
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
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
