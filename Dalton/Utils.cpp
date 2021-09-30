//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "Utils.h"

#include <Dalton/Platform.h>

#include <sstream>
#include <chrono>
#include <cstdarg>
#include <thread>

#if PLATFORM_UNIX
// getpid
# include <sys/types.h>
# include <unistd.h>
#else
# define NOMINMAX
# include <windows.h>
#endif

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

    void handle_assert_failure(const char* cond, const char* fileName, int line, const char* commentFormat, ...)
    {
        char buf [2048];
        buf[2047] = '\0';
        va_list args;
        va_start(args, commentFormat);
        vsnprintf (buf, 2047, commentFormat, args);
        va_end (args);
        
        fprintf (stderr, "ASSERT failure: %s. Condition %s failed (%s:%d)", buf, cond, fileName, line);
        abort();
    }

    void consoleMessage (const char* fmt, ...)
    {
#if PLATFORM_WINDOWS
        char buf [2048];
        buf[2047] = '\0';
        va_list args;
        va_start(args, fmt);
        vsnprintf (buf, 2047, fmt, args);
        va_end (args);
        OutputDebugString (buf);
#else
        va_list args;
        va_start(args, fmt);
        vfprintf (stderr, fmt, args);
        va_end (args);
#endif
    }
}

namespace dl
{

    double currentDateInSeconds ()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    
    std::string getUserId ()
    {
#if PLATFORM_UNIX
        return std::to_string (getuid ());
#else
        char buffer[256];
        DWORD n = 256;
        GetUserNameA (buffer, &n);
        return std::string (buffer, n);
#endif
    }

    void ScopeTimer :: start ()
    {
        _startTime = currentDateInSeconds();
    }
    
    void ScopeTimer :: stop ()
    {
        if (_startTime < 0)
            return;

        const auto endTime = currentDateInSeconds();
        const auto deltaTime = endTime - _startTime;
        
        fprintf(stderr, "[TIME] elasped in %s: %.1f ms\n", _label.c_str(), deltaTime*1e3);

        _startTime = -1.0;
    }

    void RateLimit::sleepIfNecessary(double targetDeltaTime)
    {
        const double nowTs = dl::currentDateInSeconds();
        const double timeToWait = targetDeltaTime - (nowTs - _lastCallTs);
        if (!std::isnan(timeToWait) && timeToWait > 0)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(timeToWait));
        }
        _lastCallTs = nowTs;
    }

    std::string currentThreadId ()
    {
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }

} // dl
