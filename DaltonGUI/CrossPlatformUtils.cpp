//
//  CrossPlatformUtils.cpp
//  DaltonLens
//
//  Created by Nicolas Burrus on 11/10/2020.
//  Copyright © 2020 Nicolas Burrus. All rights reserved.
//

#include "CrossPlatformUtils.h"

namespace dl
{

struct DisplayLinkTimer::Impl
{
    
};

DisplayLinkTimer::DisplayLinkTimer ()
: impl (new Impl())
{
    
}

DisplayLinkTimer::~DisplayLinkTimer ()
{
    
}

void DisplayLinkTimer::setCallback (std::function<void(void)>)
{
    
}

void DisplayLinkTimer::setEnabled (bool enabled)
{
    
}

} // dl

namespace dl
{

struct KeyboardMonitor::Impl
{
    
};

KeyboardMonitor::KeyboardMonitor ()
: impl (new Impl())
{
}

KeyboardMonitor::~KeyboardMonitor () = default;

} // dl
