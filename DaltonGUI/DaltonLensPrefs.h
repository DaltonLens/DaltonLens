//
// Copyright (c) 2021, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <memory>
#include <functional>

namespace dl
{

class DaltonLensPrefs
{
private:
    DaltonLensPrefs();
    ~DaltonLensPrefs();
    
public:
    static bool showHelpOnStartup();
    static void setShowHelpOnStartupEnabled (bool enabled);

    static int daltonizeDeficiencyKind ();
    static void setDaltonizeDeficiencyKind (int kind);
    
private:
    static DaltonLensPrefs* instance();
    
private:
    struct Impl;
    friend struct Impl;
    std::unique_ptr<Impl> impl;
};

} // dl
