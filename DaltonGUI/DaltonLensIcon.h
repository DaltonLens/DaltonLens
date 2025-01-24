//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace dl
{

// Saves the icon resource to a temporary file to feed it to tray.
class DaltonLensIcon
{
public:
    static DaltonLensIcon& instance();

private:
    DaltonLensIcon ();
    ~DaltonLensIcon ();

public:
    std::string absoluteIconPath () const;
    const uint8_t* rgba32x32 () const { return _rgba32x32.empty() ? nullptr : _rgba32x32.data(); }

private:
    std::string _absolute_png_path;
    std::vector<uint8_t> _rgba32x32;
};

} // dl