#ifndef _LS_DEFS_H_
#define _LS_DEFS_H_

#if defined(_DEBUG) || defined(DEBUG)
#define LS_DEBUG 1
#endif

#if defined(WIN32) || defined(_WIN32)
	#define LS_WINDOWS 1
	#define LS_OS "Windows"
#elif defined(__APPLE__) || defined(__MACH__)
	#define LS_DARWIN 1
	#include <TargetConditionals.h>
	#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
		#define LS_IOS 1
		#define LS_OS "ios"
	#elif defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
		#define LS_MACCATALYST 1
		#define LS_OS "maccatalyst"
	#elif defined(TARGET_OS_TV) && TARGET_OS_TV
		#define LS_TVOS 1
		#define LS_OS "tvos"
	#elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
		#define LS_WATCHOS 1
		#define LS_OS "watchos"
	#elif defined(TARGET_OS_MAC) && TARGET_OS_MAC
		#define LS_MACOS 1
		#define LS_OS "macos"
	#else
		#define LS_OS "unknown"
	#endif // TARGET_OS_IPHONE
#elif defined(__linux__)
	#define LS_LINUX 1
	#define LS_OS "linux"
#else
	#define LS_OS "unknown"
#endif // WIN32

#if _WIN64 || __x86_64__ || __ppc64__
	#define LS_x86_64 1
	#define LS_ARCH "x86_64"
#elif _WIN32 || __i386__ || __ppc__
	#define LS_x86 1
	#define LS_ARCH "x86"
#elif defined(__arm__) || defined(ARM) || defined(_ARM_)
	#define LS_ARM 1
	#define LS_ARCH "arm"
#elif (defined(__arm64__) && defined(__APPLE__)) || defined(__aarch64__)
	#define LS_ARM64 1
	#define LS_ARCH "arm64"
#else
	#define LS_ARCH "unknown"
#endif // _WIN64

typedef void *ls_handle;

#include <stdint.h>

#endif
