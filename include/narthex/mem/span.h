#ifndef NTH_MEM_SPAN_H
#define NTH_MEM_SPAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <narthex/nth_types.h>


typedef struct NthSpan {
    u8* pool;
    usize capacity;
} NthSpan;


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_SPAN_H */