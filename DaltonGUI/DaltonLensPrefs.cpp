//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "DaltonLensPrefs.h"

#include "CppUserPrefs.h"

namespace dl
{

struct DaltonLensPrefs::Impl
{
    Impl()
    : prefs("DaltonLens")
    {}
    
    CppUserPrefs prefs;
    
    struct {
        bool _showHelpOnStartup;
    } cache;
};

DaltonLensPrefs::DaltonLensPrefs()
: impl (new Impl())
{
    impl->cache._showHelpOnStartup = impl->prefs.getBool("showHelpOnStartup", true);
}

DaltonLensPrefs::~DaltonLensPrefs() = default;

DaltonLensPrefs* DaltonLensPrefs::instance()
{
    static DaltonLensPrefs prefs;
    return &prefs;
}

bool DaltonLensPrefs::showHelpOnStartup()
{
    return instance()->impl->cache._showHelpOnStartup;
}

void DaltonLensPrefs::setShowHelpOnStartupEnabled (bool enabled)
{
    if (instance()->impl->cache._showHelpOnStartup == enabled)
        return;

    instance()->impl->cache._showHelpOnStartup = enabled;
    instance()->impl->prefs.setBool("showHelpOnStartup", enabled);
    instance()->impl->prefs.sync();
}

} // dl
