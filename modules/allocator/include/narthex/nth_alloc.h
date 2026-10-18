#ifndef NTH_ALLOC_H
#define NTH_ALLOC_H

#include <narthex/nth_config.h>

#if !NTH_HAS_ALLOCATOR
#error "Narthex was built without the filesystem module"
#endif

#include <narthex/alloc/allocator.h>
#include <narthex/alloc/arena.h>
#include <narthex/alloc/dyn_arena.h>
#include <narthex/alloc/free_list.h>
#include <narthex/alloc/span.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_ALLOC_H */