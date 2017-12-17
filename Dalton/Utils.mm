//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Utils.h"

#include <unistd.h>
#include <mach/mach_time.h>
#include <mach/mach.h>

#include <Foundation/Foundation.h>

namespace dl
{
    std::string formatted (const char* fmt, ...)
    {
        char buf [2048];
        buf[2047] = '\0';
        va_list args;
        va_start(args, fmt);
        vsnprintf (buf, 2047, fmt, args);
        va_end (args);
        return buf;
    }
}

namespace dl
{

    double currentDateInSeconds ()
    {
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        
        uint64_t newTime = mach_absolute_time();
        
        return ((double)newTime*timebase.numer)/((double)timebase.denom *1e9);
    }
    
    void ScopeTimer :: start ()
    {
        _startTime = currentDateInSeconds();
    }
    
    void ScopeTimer :: stop ()
    {
        const auto endTime = currentDateInSeconds();
        const auto deltaTime = endTime - _startTime;
        
        NSLog(@"[TIME] elasped in %s: %.1f ms", _label.c_str(), deltaTime*1e3);
    }

} // dl
