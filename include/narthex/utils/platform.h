#ifndef NTH_UTILS_PLATFORM_H
#define NTH_UTILS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif


#define NTH_PLATFORM_WINDOWS 0
#define NTH_PLATFORM_LINUX   0
#define NTH_PLATFORM_UNKNOWN 0

#if defined(_WIN32) || defined(_WIN64)
    #undef  NTH_PLATFORM_WINDOWS
    #define NTH_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #undef  NTH_PLATFORM_LINUX
    #define NTH_PLATFORM_LINUX 1
#else
    #undef  NTH_PLATFORM_UNKNOWN
    #define NTH_PLATFORM_UNKNOWN 1
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_PLATFORM_H */