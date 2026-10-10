#include "narthex/nth_sequencer.h"

typedef struct NthSeqSystemSlot {
    NthSeqSystemDesc desc;
    NthSeqTick tick_count;
} NthSeqSystemSlot;

typedef struct NthSeqSnapshotSlot {
    NthSeqMessage **messages; // the array of messages per system
    nth_usize *messages_size; // the size of each array
    void **states; // the pointer to the state of each system

    NthUptimeNs start_time;
} NthSeqSnapshotSlot;