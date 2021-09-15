//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

namespace dl
{

// Components can send notifications to the controller.
struct ImageViewerObserver
{
    virtual void onDismissRequested () = 0;
    virtual void onHelpRequested () = 0;
    virtual void onControlsRequested () = 0;
};

} // dl
