//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <Dalton/OpenGL.h>
#include <Dalton/Filters.h>

namespace dl
{

class ImguiImageFilter
{
public:
    ImguiImageFilter();
    ~ImguiImageFilter();
    
    void enable (GLFilter* filter);
    void disable ();

private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
