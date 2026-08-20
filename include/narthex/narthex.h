#ifndef NARTHEX_H
#define NARTHEX_H

#include <narthex/nth_types.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthLoggerDesc {
    nth_usize buffer_size; /* 0 selects the default */
    nth_b8    flush_each;
} NthLoggerDesc;

typedef struct NthCoreDesc {
    NthLoggerDesc logger;
    const char *app_name; /* NULL is valid */
} NthCoreDesc;


NTH_API NthResult nth_init(const NthCoreDesc *desc);
NTH_API void nth_term(void);


#ifdef __cplusplus
}
#endif

#endif /* NARTHEX_H */