#ifndef NTH_UTILS_BUILD_H
#define NTH_UTILS_BUILD_H

#ifdef __cplusplus
extern "C" {
#endif


#define NTH_DEBUG   0
#define NTH_RELEASE 0

#ifdef NDEBUG
    #undef NTH_RELEASE
    #define NTH_RELEASE 1
#else
    #undef NTH_DEBUG
    #define NTH_DEBUG 1
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_BUILD_H */