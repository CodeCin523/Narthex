#ifndef NTH_UTILS_API_H
#define NTH_UTILS_API_H

#include <narthex/nth_config.h>
#include <narthex/utils/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Marks a symbol as part of the public ABI. NTH_BUILDING is set only while
   compiling narthex itself, never for a consumer. */
#if !NTH_SHARED
    #define NTH_API
#elif NTH_COMPILER_MSVC
    #ifdef NTH_BUILDING
        #define NTH_API __declspec(dllexport)
    #else
        #define NTH_API __declspec(dllimport)
    #endif
#elif NTH_COMPILER_GCC || NTH_COMPILER_CLANG
    #define NTH_API __attribute__((visibility("default")))
#else
    #define NTH_API
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_UTILS_API_H */
