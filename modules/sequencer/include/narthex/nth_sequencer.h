#ifndef NTH_SEQUENCER_H
#define NTH_SEQUENCER_H

#include <narthex/nth_types.h>
#include <narthex/nth_uptime.h>
#include <narthex/utils/compiler.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthSeqSystemId {
    nth_u32 id;
} NthSeqSystemId;
typedef struct NthSeqPromiseId {
    nth_u16 idx;
    nth_u16 gen;
}  NthSeqPromiseId;
typedef nth_u64 NthSeqTick;

typedef struct NthSeqTarget {
    NthSeqTick      tick;
    NthSeqSystemId  system;
} NthSeqTarget;
typedef struct NthSeqPayload {
    nth_u32 kind;
    nth_u32 meta;

    nth_u8  payload[24];
} NthSeqPayload;
typedef struct NthSeqMessage {
    NthSeqTarget target;
    NthSeqPayload payload;
} NthSeqMessage;

typedef struct NthSeqView {
    NthSeqSystemId id;
    const void *state;
} NthSeqView;

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
typedef struct NthSequencerDesc {
    nth_u32 system_capacity;

    nth_u32 max_messages;
    nth_u32 max_promises;
    nth_u32 max_snapshots;

    NthDeltaNs promise_timeout;
} NthSequencerDesc;


// TLDR; The sequencer is a deterministic execution timeline for systems.
NTH_API NthResult nth_init_sequencer(const NthSequencerDesc *desc);
NTH_API void nth_term_sequencer(void);

NTH_API NthResult nth_seq_register_system(const NthSeqSystemDesc *desc, NthSeqSystemId *out_id);
NTH_API void nth_seq_unregister_system(NthSeqSystemId id);

NTH_API void nth_seq_start(void);
NTH_API void nth_seq_advance(void);

NTH_API NthResult nth_seq_message(NthSeqTarget target, NthSeqPayload payload);
NTH_API NthResult nth_seq_promise(NthSeqTarget target, NthSeqPromiseId *out_id);
NTH_API NthResult nth_seq_fulfill(NthSeqPromiseId id, NthSeqPayload payload);

NTH_API NthSeqView nth_seq_get_view(NthSeqSystemId id);
NTH_API NthSeqTick nth_seq_get_tick(NthSeqSystemId id);
NTH_API NthDeltaNs nth_seq_get_period(NthSeqSystemId id);
NTH_API nth_f64 nth_seq_get_alpha(NthSeqSystemId id);


#ifdef __cplusplus
}
#endif

#endif /* NTH_SEQUENCER_H */