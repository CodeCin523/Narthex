#ifndef NTH_SEQUENCER_H
#define NTH_SEQUENCER_H

#include <narthex/nth_types.h>
#include <narthex/nth_uptime.h>
#include <narthex/utils/compiler.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthSeqSystemIdx {
    nth_u16 idx;
    nth_u16 gen;
} NthSeqSystemIdx;
typedef struct NthSeqProviderIdx {
    nth_u16 idx;
    nth_u16 gen;
}  NthSeqProviderIdx;
typedef struct NthSeqPromiseIdx {
    nth_u16 idx;
    nth_u16 gen;
}  NthSeqPromiseIdx;
typedef nth_u64 NthSeqTick;

typedef struct NthSeqOrigin {
    NthSeqTick          t_tick;
    NthSeqSystemIdx     t_idx;
    NthSeqProviderIdx   s_idx;
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
    NthSeqSystemIdx idx;
    const void *state;
} NthSeqView;

typedef void (*NthSeqTickPfn)(
    void *state_prev, void *state_next,
    const NthSeqMessage *commands, nth_u64 command_count,
    const NthSeqView *views, nth_u64 view_count,
    NthSeqTick tick, void *userdata);
typedef void (*NthSeqCopyPfn)(void **dest, void *state, void *userdata);
typedef void (*NthSeqReleasePfn)(void *state, void *userdata);

typedef void (*NthSeqPumpPfn)(NthDeltaNs dt, void *userdata);


typedef struct NthSeqSystemDesc {

} NthSeqSystemDesc;
typedef struct NthSeqProviderDesc {

} NthSeqProviderDesc;

typedef struct NthSequencerDesc {

} NthSequencerDesc;


NTH_API NthResult nth_init_sequencer(const NthSequencerDesc *desc);
NTH_API void nth_term_sequencer(void);

NTH_API NthResult nth_seq_register_system(const NthSeqSystemDesc *desc, NthSeqSystemIdx *out_idx);
NTH_API void nth_seq_unregister_system(NthSeqSystemIdx idx);

NTH_API NthResult nth_seq_register_provider(const NthSeqProviderDesc *desc, NthSeqProviderIdx *out_idx);
NTH_API void nth_seq_unregister_provider(NthSeqProviderIdx idx);

// probably should replace nthresult by a custom PostStatus or PostResult, we will see when there
NTH_API NthResult nth_seq_post(NthSeqOrigin origin, NthSeqPayload payload);

NTH_API NthResult nth_seq_promise(NthSeqOrigin, NthSeqPromiseIdx *out_idx);
NTH_API NthResult nth_seq_fulfill(NthSeqPromiseIdx idx, NthSeqPayload payload);

NTH_API void nth_seq_pump_all(NthDeltaNs dt);
NTH_API void nth_seq_pump_one(NthSeqProviderIdx idx, NthDeltaNs dt);
NTH_API void nth_seq_advance(NthUptimeNs up_time); // both uptime and delta makes sense here, one is just simpler than the other, though since both are a trustable source of information... I don't know yet

NTH_API const void *nth_seq_get_state(NthSeqSystemIdx idx);
NTH_API const void *nth_seq_get_prev_state(NthSeqSystemIdx idx);
NTH_API NthSeqTick nth_seq_get_tick_count(NthSeqSystemIdx idx);
NTH_API nth_f64 nth_seq_get_alpha(NthSeqSystemIdx idx);


#ifdef __cplusplus
}
#endif

#endif /* NTH_SEQUENCER_H */