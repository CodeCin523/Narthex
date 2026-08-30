#include <narthex/mem/free_list.h>

#include <narthex/utils/check.h>
#include <narthex/inl/align.h>

#include "poison.h"

#include <stdalign.h>
#include <stdlib.h>


typedef struct NTH_ALIGNAS(alignof(max_align_t)) NthFreeListMeta {
    struct NthFreeListMeta *next;
    nth_usize key; // size && span_idx
} NthFreeListMeta;

_Static_assert(sizeof(NthFreeListMeta) == 16, "Meta not correctly alligned");


#define SIZEUP_TO_CSIZE(size) ((nth_usize)(((size) + 15) >> 4))
#define SIZEDOWN_TO_CSIZE(size) ((nth_usize)((size) >> 4))

#define FLIST_CSIZE_SHIFT 0
#define FLIST_SPAN_IDX_SHIFT 48
#define FLIST_CSIZE_MASK ((nth_usize)0xFFFFFFFFFFFF)
#define FLIST_SPAN_IDX_MASK ((nth_usize)0xFFFF)

#define FLIST_CSIZE_MAX    ((nth_usize)FLIST_CSIZE_MASK)
#define FLIST_SPAN_IDX_MAX ((nth_usize)FLIST_SPAN_IDX_MASK)


#define FLIST_CSIZE(k)    ((nth_usize)(((k) >> FLIST_CSIZE_SHIFT) & FLIST_CSIZE_MASK))
#define FLIST_SPAN_IDX(k) ((nth_usize)(((k) >> FLIST_SPAN_IDX_SHIFT) & FLIST_SPAN_IDX_MASK))
#define FLIST_SET_CSIZE(k, v)    ((k) = ((k) & ~(FLIST_CSIZE_MASK << FLIST_CSIZE_SHIFT)) | (((nth_usize)(v) & FLIST_CSIZE_MASK) << FLIST_CSIZE_SHIFT))
#define FLIST_SET_SPAN_IDX(k, v) ((k) = ((k) & ~(FLIST_SPAN_IDX_MASK << FLIST_SPAN_IDX_SHIFT)) | (((nth_usize)(v) & FLIST_SPAN_IDX_MASK) << FLIST_SPAN_IDX_SHIFT))


static inline void nth_free_list_add_free(NthFreeList *list, NthFreeListMeta *meta) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(meta != NULL));

    NthFreeListMeta *prev = NULL;
    NthFreeListMeta *cur = list->p_free;

    while (cur != NULL && cur < meta) {
        prev = cur;
        cur = cur->next;
    }

    meta->next = cur;
    if (prev == NULL) {
        list->p_free = meta;
    } else {
        prev->next = meta;
    }

    // Coalesce with the following block if contiguous
    if (cur != NULL && FLIST_SPAN_IDX(meta->key) == FLIST_SPAN_IDX(cur->key) &&
        (NthFreeListMeta *)((char *)meta + FLIST_CSIZE(meta->key) * sizeof(NthFreeListMeta)) == cur) {
        FLIST_SET_CSIZE(meta->key, FLIST_CSIZE(meta->key) + FLIST_CSIZE(cur->key));
        meta->next = cur->next;
    }

    // Coalesce with the preceding block if contiguous
    if (prev != NULL && FLIST_SPAN_IDX(meta->key) == FLIST_SPAN_IDX(prev->key) &&
        (NthFreeListMeta *)((char *)prev + FLIST_CSIZE(prev->key) * sizeof(NthFreeListMeta)) == meta) {
        FLIST_SET_CSIZE(prev->key, FLIST_CSIZE(meta->key) + FLIST_CSIZE(prev->key));
        prev->next = meta->next;
    }
}
static inline NthFreeListMeta *free_list_carve_span(NthSpan span, nth_usize span_idx) {
    nth_uptr pad = nth_align_pad((nth_uptr)span.base, alignof(NthFreeListMeta));

    NTH_DASSERT(NTH_LIKELY(pad < span.size));
    nth_usize avail = span.size - pad;
    NTH_DASSERT(NTH_LIKELY(avail >= sizeof(NthFreeListMeta)));

    NthFreeListMeta *meta = (NthFreeListMeta *)(span.base + pad);

    meta->next = NULL;
    FLIST_SET_CSIZE(meta->key, SIZEDOWN_TO_CSIZE(avail));
    FLIST_SET_SPAN_IDX(meta->key, span_idx);

    nth_usize data_size = (FLIST_CSIZE(meta->key)-1) * sizeof(NthFreeListMeta);
    nth_poison_dead(meta + 1, data_size);

    return meta;
}


/* ================================================================================ */
/*  IMPLEMENTATION                                                                  */
/* ================================================================================ */

nth_b8 nth_free_list_grow(NthFreeList *list, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(span.base != NULL && span.size != 0));
    NTH_DASSERT(NTH_LIKELY(SIZEUP_TO_CSIZE(span.size) < FLIST_CSIZE_MAX));

    if(NTH_UNLIKELY(list->span_count >= list->span_capacity)) {
        nth_usize ncapacity = list->span_capacity * 2;

        NthSpan *tmp = realloc(list->spans, ncapacity * sizeof(NthSpan));
        if(NTH_UNLIKELY(tmp == NULL))
            return NTH_FALSE;

        list->spans = tmp;
        list->span_capacity = ncapacity;
    }

    nth_usize idx = list->span_count;
    list->spans[idx] = span;
    list->span_count++;

    
    NthFreeListMeta *meta = free_list_carve_span(span, idx);
    nth_free_list_add_free(list, meta);

    return NTH_TRUE;
}
NthSpan nth_free_list_shrink(NthFreeList *list) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));

    if (list->span_count == 0)
        return (NthSpan){0};

    nth_usize last = list->span_count - 1;
    NthSpan ret = list->spans[last];

    nth_uptr span_start = (nth_uptr)ret.base;
    nth_uptr span_end = span_start + ret.size;

    // Refuse to shrink if any live allocation lies within this span
    NthFreeListMeta *curr = list->p_used;
    while (curr != NULL) {
        nth_uptr addr = (nth_uptr)curr;
        if (addr >= span_start && addr < span_end)
            return (NthSpan){0};
        if(FLIST_SPAN_IDX(curr->key) == last)
            return (NthSpan){0};

        curr = curr->next;
    }

    // Strip any free blocks belonging to this span out of the free list
    NthFreeListMeta **prev_ptr = (NthFreeListMeta **) &list->p_free;
    NthFreeListMeta *node = list->p_free;
    while (node != NULL) {
        nth_uptr addr = (nth_uptr)node;
        if (addr >= span_start && addr < span_end) {
            *prev_ptr = node->next;
            node = *prev_ptr;
        } else {
            prev_ptr = &node->next;
            node = node->next;
        }
    }

    list->spans[last] = (NthSpan){0};
    list->span_count = last;

    nth_poison_disown(ret.base, ret.size);

    return ret;
}

void *nth_free_list_alloc(NthFreeList *list, nth_usize size) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));
    NTH_DASSERT(NTH_LIKELY(size > 0));

    const nth_usize csize = SIZEUP_TO_CSIZE(size) + 1;
    NTH_DASSERT(NTH_LIKELY(csize < FLIST_CSIZE_MAX));

    // Get possible address
    NthFreeListMeta **last_ptr = (NthFreeListMeta **)&list->p_free;
    NthFreeListMeta *last = (NthFreeListMeta *)list->p_free;
    
    while(last != NULL) {
        if(FLIST_CSIZE(last->key) >= csize)
            break;

        last_ptr = &last->next;
        last = last->next;
    }
    if(NTH_UNLIKELY(last == NULL))
        return NULL;

    // Subdivide if possible
    nth_usize alloc_csize;

    if(FLIST_CSIZE(last->key) > csize+1) {
        NthFreeListMeta *next = last + csize;

        nth_poison_live(next, sizeof(NthFreeListMeta));

        *last_ptr = next;

        next->next = last->next;
        FLIST_SET_CSIZE(next->key, FLIST_CSIZE(last->key) - csize);
        FLIST_SET_SPAN_IDX(next->key, FLIST_SPAN_IDX(last->key));

        alloc_csize = csize;
    } else {
        *last_ptr = last->next;

        alloc_csize = FLIST_CSIZE(last->key);
    }

    // Last step
    FLIST_SET_CSIZE(last->key, alloc_csize);
    last->next = list->p_used;
    list->p_used = last;

    ++last;
    nth_poison_live(last, (alloc_csize-1) * sizeof(NthFreeListMeta));

    return last;
}
void nth_free_list_free(NthFreeList *list, void *addr) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));

    NthFreeListMeta *head = (NthFreeListMeta *)addr - 1;

    // Check to make sure it exists
    NthFreeListMeta **last_ptr = (NthFreeListMeta **) &list->p_used;
    NthFreeListMeta *last = list->p_used;

    while (last != NULL) {
        if (last == head)
            break;

        last_ptr = &last->next;
        last = last->next;
    }
    if (last == NULL)
        return;

    // It is valid
    *last_ptr = head->next;

    nth_poison_dead(addr, (FLIST_CSIZE(head->key)-1) * sizeof(NthFreeListMeta));

    // Place in free list
    nth_free_list_add_free(list, head);
}
void nth_free_list_clear(NthFreeList *list) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));

    list->p_used = NULL;
    list->p_free = NULL;

    for(nth_usize i = 0; i < list->span_count; i++) {
        NthSpan span = list->spans[i];
        
        NthFreeListMeta *meta = free_list_carve_span(span, i);
        nth_free_list_add_free(list, meta);
    }
}


/* ================================================================================ */
/*  ALLOCATOR                                                                       */
/* ================================================================================ */

static void *free_list_alloc_alloc(const void *ctx, nth_usize size, nth_usize align) {
    (void)align;
    return nth_free_list_alloc((NthFreeList *)ctx, size);
}
static void free_list_alloc_free(const void *ctx, void *ptr) {
    nth_free_list_free((NthFreeList *)ctx, ptr);
}
static void *free_list_alloc_realloc(const void *ctx, void *ptr, nth_usize size, nth_usize align) {
    if (ptr == NULL)
        return free_list_alloc_alloc(ctx, size, align);

    if (size == 0) {
        free_list_alloc_free(ctx, ptr);
        return NULL;
    }

    NthFreeListMeta *head = (NthFreeListMeta *)ptr - 1;
    nth_usize old_size = (FLIST_CSIZE(head->key)-1) * sizeof(NthFreeListMeta);

    void *new_ptr = free_list_alloc_alloc(ctx, size, align);
    if (NTH_UNLIKELY(new_ptr == NULL))
        return NULL;

    memcpy(new_ptr, ptr, old_size < size ? old_size : size);

    free_list_alloc_free(ctx, ptr);

    return new_ptr;
}
static void free_list_alloc_clear(const void *ctx) {
    nth_free_list_clear((NthFreeList *)ctx);
}

NthAllocator nth_free_list_as_allocator(NthFreeList *list) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(list->spans != NULL));

    return (NthAllocator) {
        .ctx = list,
        .alloc = free_list_alloc_alloc,
        .free = free_list_alloc_free,
        .realloc = free_list_alloc_realloc,
        .clear = free_list_alloc_clear
    };
}


/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

NthResult nth_setup_free_list(NthFreeList *list, NthSpan span) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));
    NTH_DASSERT(NTH_LIKELY(span.base != NULL && span.size != 0));

    NthSpan *tmp = malloc(4 * sizeof(NthSpan));
    if(NTH_UNLIKELY(tmp == NULL))
        return NTH_RESULT_OUT_OF_MEMORY;

    list->spans = tmp;
    list->span_count = 1;
    list->span_capacity = 4;

    list->spans[0] = span;

    list->p_free = NULL;
    list->p_used = NULL;

    NthFreeListMeta *meta = free_list_carve_span(span, 0);
    nth_free_list_add_free(list, meta);

    return NTH_RESULT_OK;
}
void nth_teardown_free_list(NthFreeList *list) {
    NTH_DASSERT(NTH_LIKELY(list != NULL));

    if(list->spans != NULL) {
        for(nth_usize i = 0; i < list->span_count; i++)
            nth_poison_disown(list->spans[i].base, list->spans[i].size);

        free(list->spans);
    }

    list->p_free = NULL;
    list->p_used = NULL;
    list->spans = NULL;
    list->span_count = 0;
    list->span_capacity = 0;
}