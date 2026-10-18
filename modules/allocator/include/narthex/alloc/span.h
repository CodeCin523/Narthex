#ifndef NTH_ALLOC_SPAN_H
#define NTH_ALLOC_SPAN_H

#include <narthex/nth_config.h>

#if !NTH_HAS_ALLOCATOR
#error "Narthex was built without the filesystem module"
#endif

#include <narthex/nth_types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthSpan {
    nth_u8   *base;
    nth_usize size;
} NthSpan;


#ifdef __cplusplus
}
#endif

#endif /* NTH_ALLOC_SPAN_H */