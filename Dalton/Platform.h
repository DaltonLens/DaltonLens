//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#if __APPLE__
# include <TargetConditionals.h>
#endif

#ifdef __linux__
# define PLATFORM_LINUX 1
#endif

#if TARGET_OS_OSX
# define PLATFORM_MACOS 1
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
# define PLATFORM_WINDOWS 1
#endif

#if PLATFORM_MACOS || PLATFORM_LINUX
# define PLATFORM_UNIX 1
#endif
