#ifndef NTH_SEQ_TYPES_H
#define NTH_SEQ_TYPES_H

#include <narthex/nth_types.h>

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
    const NthSeqSystemId id;
    const NthSeqTick tick;
    const void *state;
} NthSeqView;


#ifdef __cplusplus
}
#endif

#endif /* NTH_SEQ_TYPES_H */