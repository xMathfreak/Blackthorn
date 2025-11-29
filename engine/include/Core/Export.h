#pragma once

#ifdef _WIN32
	#ifdef BLACKTHORN_EXPORTS
		#define BLACKTHORN_API __declspec(dllexport)
	#else
		#define BLACKTHORN_API __declspec(dllimport)
	#endif
#else
	#ifdef BLACKTHORN_EXPORTS
		#define BLACKTHORN_API __attribute__((visibility("default")))
	#else
		#define BLACKTHORN_API
	#endif
#endif