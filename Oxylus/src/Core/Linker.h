#pragma once

#include "PlatformDetection.h"

#ifdef OX_PLATFORM_WINDOWS
	#ifdef OX_BUILD_DLL
		#define OX_API __declspec(dllexport)
	#else
		#define OX_API __declspec(dllimport)
	#endif
#else
    #define OX_API
#endif