#ifndef NTH_SEQ_CORE_H
#define NTH_SEQ_CORE_H

#include <narthex/sequencer/types.h>
#include <narthex/nth_uptime.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*NthSeqTickPfn)(
    const NthSeqMessage *messages, nth_u64 message_count,
    const NthSeqView *views, nth_u64 view_count,
    NthSeqTick tick, void *state, const void *userdata);
typedef void (*NthSeqSnapshotPfn)(void **snapshot, void *state, const void *userdata);
typedef void (*NthSeqReleasePfn)(void *state, const void *userdata);

typedef struct NthSeqSystemDesc {
    NthDeltaNs period;
    nth_u32 priority;
    nth_u32 max_catchup;

    NthSeqTickPfn pfn_tick;
    NthSeqSnapshotPfn pfn_snapshot;
    NthSeqReleasePfn pfn_release;

    void *state;
    void *userdata;

    NthSeqSystemId *views;
    nth_u64 view_count;
} NthSeqSystemDesc;

/* typedef struct NSequencerDesc {
    nth_u32 system_capacity;

    nth_u32 max_messages;
    nth_u32 max_promises;
    nth_u32 max_snapshots;

    NthDeltaNs promise_timeout;
} NSequencerDesc; */


// TLDR; The sequencer is a deterministic execution timeline for systems.
NTH_API NthResult nth_init_sequencer(void);
NTH_API void nth_term_sequencer(void);

NTH_API NthResult nth_seq_register_system(const NthSeqSystemDesc *desc, NthSeqSystemId *out_id);
NTH_API void nth_seq_unregister_system(NthSeqSystemId id);

NTH_API NthSeqSystemDesc *nth_seq_get_desc(NthSeqSystemId id);


#ifdef __cplusplus
}
#endif

#endif /* NTH_SEQ_CORE_H */