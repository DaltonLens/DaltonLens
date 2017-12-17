//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <cstdio>
#include <string>

#define DL_MULTI_STATEMENT_MACRO(X) do { X } while(0)

#if DEBUG
#define dl_dbg(...) DL_MULTI_STATEMENT_MACRO ( fprintf (stderr, "DEBUG: "); fprintf (stderr, __VA_ARGS__); fprintf (stderr, "\n"); )
#else
#define dl_dbg(...)
#endif // !DEBUG

namespace dl
{
    
    std::string formatted (const char* fmt, ...);    
    
} // dl

namespace dl
{

    double currentDateInSeconds ();
    
    struct ScopeTimer
    {
        ScopeTimer (const char* label) : _label (label)
        {
            start ();
        }
        
        ~ScopeTimer () { stop (); }
        
        void start ();
        void stop ();
        
    private:
        std::string _label;
        double _startTime = 0.;
    };
    
}
