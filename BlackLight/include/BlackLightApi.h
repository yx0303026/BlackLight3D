#pragma once

/// Controls symbol visibility for the BlackLight Engine SDK.
#if defined(_WIN32)
	#if defined(BLACKLIGHT_STATIC)
		#define BLACKLIGHT_API
	#elif defined(BLACKLIGHT_BUILD_SHARED)
		#define BLACKLIGHT_API __declspec(dllexport)
	#else
		#define BLACKLIGHT_API __declspec(dllimport)
	#endif
#elif defined(BLACKLIGHT_BUILD_SHARED)
	#define BLACKLIGHT_API __attribute__((visibility("default")))
#else
	#define BLACKLIGHT_API
#endif

/// MSVC reports C4251 for every exported class that stores a standard-library
/// type. The engine deliberately shares these types with its clients, so the
/// warning is not actionable; the macros below scope the suppression to the
/// affected class declarations instead of disabling it for the whole build.
#if defined(_MSC_VER)
	#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH \
		__pragma(warning(push)) __pragma(warning(disable : 4251))
	#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP __pragma(warning(pop))
#else
	#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH
	#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP
#endif
