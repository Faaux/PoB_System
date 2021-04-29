#pragma once

#if defined( _MSC_VER )
#define CORO_COMPILER_MSVC _MSC_FULL_VER
#else
#define CORO_COMPILER_MSVC 0
#endif

#if defined( _WIN32_WINNT ) || defined( _WIN32 )
#if !defined( _WIN32_WINNT )
// Default to targeting Windows 10 if not defined.
#define _WIN32_WINNT 0x0A00
#endif
#define CORO_WINDOWS _WIN32_WINNT
#else
#define CORO_WINDOWS 0
#endif