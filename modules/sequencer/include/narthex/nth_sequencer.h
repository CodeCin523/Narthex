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
typedef struct NthSeqProviderId {
    nth_u32 id;
}  NthSeqProviderId;
typedef struct NthSeqPromiseId {
    nth_u16 idx;
    nth_u16 gen;
}  NthSeqPromiseId;
typedef nth_u64 NthSeqTick;

typedef nth_u8 NthSeqOriginKind;
enum { NTH_SEQ_ORIGIN_SYSTEM = 0, NTH_SEQ_ORIGIN_PROVIDER = 1 };

typedef struct NthSeqOrigin {
    NthSeqTick          t_tick;
    NthSeqSystemId      t_id;

    nth_u32 meta;

    NthSeqOriginKind s_kind;
    nth_u8 reserved[3];

    union {
        NthSeqSystemId system;
        NthSeqProviderId provider;
    } s;
} NthSeqOrigin;
typedef struct NthSeqPayload {
    nth_u32 kind;
    nth_u32 meta;

    nth_u8  payload[24];
} NthSeqPayload;
typedef struct NthSeqMessage {
    NthSeqOrigin origin;
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

typedef void (*NthSeqPumpPfn)(NthDeltaNs dt, void *userdata);


typedef struct NthSeqSystemDesc {
    NthDeltaNs hz;
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
typedef struct NthSeqProviderDesc { // feel kinda empty
    NthSeqPumpPfn pfn_pump;

    void *userdata;
} NthSeqProviderDesc;

typedef struct NthSequencerDesc { // I do not like the names of those field, even if they are instrumental. if 0 = infinite. command and pomise are per-system limits.
    nth_u32 system_capacity;
    nth_u32 producer_capacity;
    nth_u32 command_capacity;
    nth_u32 promise_capacity;
} NthSequencerDesc;


NTH_API NthResult nth_init_sequencer(const NthSequencerDesc *desc);
NTH_API void nth_term_sequencer(void);

NTH_API NthResult nth_seq_register_system(const NthSeqSystemDesc *desc, NthSeqSystemId *out_id);
NTH_API void nth_seq_unregister_system(NthSeqSystemId id);

NTH_API NthResult nth_seq_register_provider(const NthSeqProviderDesc *desc, NthSeqProviderId *out_id);
NTH_API void nth_seq_unregister_provider(NthSeqProviderId id);

// probably should replace nthresult by a custom PostStatus or PostResult, we will see when there
NTH_API NthResult nth_seq_post(NthSeqOrigin origin, NthSeqPayload payload);

NTH_API NthResult nth_seq_promise(NthSeqOrigin origin, NthSeqPromiseId *out_id);
NTH_API NthResult nth_seq_fulfill(NthSeqPromiseId id, NthSeqPayload payload);

NTH_API void nth_seq_pump_all(NthDeltaNs dt);
NTH_API void nth_seq_pump_one(NthSeqProviderId id, NthDeltaNs dt);
NTH_API void nth_seq_advance(NthUptimeNs up_time);

NTH_API const void *nth_seq_get_state(NthSeqSystemId id);
NTH_API NthSeqTick nth_seq_get_tick(NthSeqSystemId id);
NTH_API NthDeltaNs nth_seq_get_hz(NthSeqSystemId id);
NTH_API nth_f64 nth_seq_get_alpha(NthSeqSystemId id);


#ifdef __cplusplus
}
#endif

#endif /* NTH_SEQUENCER_H */