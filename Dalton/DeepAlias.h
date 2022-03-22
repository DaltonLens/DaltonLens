//
// Copyright (c) 2022, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <Dalton/Image.h>

#include <memory>

namespace dl
{

class DeepAlias
{
public:
    DeepAlias ();
    ~DeepAlias ();

    ImageSRGBA undoAntiAliasing (const ImageSRGBA& inputRgba);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
