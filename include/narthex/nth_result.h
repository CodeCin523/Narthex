#ifndef NTH_RESULT_H
#define NTH_RESULT_H

#include <narthex/nth_types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u32 NthResult;
enum {
    // core / general
    NTH_RESULT_OK                           = 0x00,
    NTH_RESULT_FAILED                       = 0x01,
    NTH_RESULT_OUT_OF_MEMORY                = 0x02,
    NTH_RESULT_CAPACITY_FULL                = 0x03,
    NTH_RESULT_NOT_FOUND                    = 0x04,
    NTH_RESULT_NOT_SUPPORTED                = 0x05,
    NTH_RESULT_OVERFLOW                     = 0x06,

    // arguments / config
    NTH_RESULT_INVALID_ARGUMENT             = 0x10,
    NTH_RESULT_MISSING_OUTPUT               = 0x11,
    NTH_RESULT_CONFIG_REQUIRED              = 0x12,
    NTH_RESULT_INVALID_RANGE                = 0x13,
    NTH_RESULT_INVALID_ALIGNMENT            = 0x14,

    // life-cycle / state
    NTH_RESULT_INVALID_STATE                = 0x20,
    NTH_RESULT_NOT_INITIALIZED              = 0x21,
    NTH_RESULT_ALREADY_INITIALIZED          = 0x22,
    NTH_RESULT_DEPENDENCY_UNINITIALIZED     = 0x23,

    // external / runtime
    NTH_RESULT_PLATFORM_ERROR               = 0x30,
    NTH_RESULT_BACKEND_ERROR                = 0x31,
    NTH_RESULT_TIMEOUT                      = 0x32,
    NTH_RESULT_NOT_READY                    = 0x33,
    NTH_RESULT_IO_ERROR                     = 0x34,
    NTH_RESULT_PERMISSION_DENIED            = 0x35,
};


#ifdef __cplusplus
}
#endif

#endif /* NTH_RESULT_H */