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
