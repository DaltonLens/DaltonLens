//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <cstdio>
#include <string>

#define DL_MULTI_STATEMENT_MACRO(X) do { X } while(0)

#ifndef NDEBUG
#define dl_dbg(...) DL_MULTI_STATEMENT_MACRO ( fprintf (stderr, "DEBUG: "); fprintf (stderr, __VA_ARGS__); fprintf (stderr, "\n"); )
#else
#define dl_dbg(...)
#endif // !DEBUG

#ifndef NDEBUG
#define dl_assert(cond, ...) DL_MULTI_STATEMENT_MACRO ( if (!(cond)) dl::handle_assert_failure(#cond, __FILE__, __LINE__, __VA_ARGS__); else {} )
#else
#define dl_assert(...)
#endif // !DEBUG

namespace dl
{
        
    std::string formatted (const char* fmt, ...);

    void handle_assert_failure(const char* cond, const char* fileName, int line, const char* fmt, ...);
    
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
        double _startTime = -1;
    };
    
}
