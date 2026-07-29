#ifndef NTH_MEM_SPAN_H
#define NTH_MEM_SPAN_H

#include <narthex/nth_types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthSpan {
    nth_u8* pool;
    nth_usize capacity;
} NthSpan;


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_SPAN_H */