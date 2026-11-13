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

/// MSVC reports C4251 for exported classes that store standard-library types
/// and C4275 for exported exceptions derived from the standard library. The
/// engine deliberately shares these types with its clients, so the warnings
/// are not actionable; these macros scope suppression to affected declarations.
#if defined(_MSC_VER)
#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH \
	__pragma(warning(push)) __pragma(warning(disable : 4251 4275))
#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP __pragma(warning(pop))
#else
#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_PUSH
#define BLACKLIGHT_DETAIL_DLL_INTERFACE_WARNING_POP
#endif
