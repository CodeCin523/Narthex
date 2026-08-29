#ifndef NTH_MEM_FREE_LIST_H
#define NTH_MEM_FREE_LIST_H

#include <narthex/nth_types.h>
#include <narthex/mem/allocator.h>
#include <narthex/mem/span.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthFreeList {
    void *p_free;
    void *p_used;

    NthSpan *spans;
    nth_usize span_count;
    nth_usize span_capacity;
} NthFreeList;


NTH_API NthResult nth_setup_free_list(NthFreeList *list, NthSpan span);
NTH_API void nth_teardown_free_list(NthFreeList *list);

NTH_API nth_b8 nth_free_list_grow(NthFreeList *list, NthSpan span);
NTH_API NthSpan nth_free_list_shrink(NthFreeList *list);

NTH_API void *nth_free_list_alloc(NthFreeList *list, nth_usize size);
NTH_API void nth_free_list_free(NthFreeList *list, void *addr); // I do not like that function name.
NTH_API void nth_free_list_clear(NthFreeList *list);

NTH_API NthAllocator nth_free_list_as_allocator(NthFreeList *list);


#ifdef __cplusplus
}
#endif

#endif /* NTH_MEM_FREE_LIST_H */