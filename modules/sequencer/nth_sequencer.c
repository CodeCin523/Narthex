#include <narthex/nth_sequencer.h>

#include <narthex/utils/check.h>


typedef nth_usize NthSeqSize;

#define NTH_SEQ_SIZE_MASK (NTH_USIZE_MAX >> 1)
#define NTH_SEQ_SIZE_FLAG (~NTH_SEQ_SIZE_MASK)

#define NTH_SEQ_SIZE_GFLAG(s)      ((s) & NTH_SEQ_SIZE_FLAG)
#define NTH_SEQ_SIZE_SFLAG(s, v)   ((s) & ~NTH_SEQ_SIZE_FLAG | ((v) ? NTH_SEQ_SIZE_FLAG : 0))

#define NTH_SEQ_SIZE_GSZIE(s)      ((s) & NTH_SEQ_SIZE_MASK)
#define NTH_SEQ_SIZE_SSZIE(s, v)   ((s) & ~NTH_SEQ_SIZE_MASK | ((v) & NTH_SEQ_SIZE_MASK))


typedef struct NthSeqSystemSlot {
    NthSeqSystemDesc desc;

    NthSeqMessage *messages;
    NthSeqSize message_count;
} NthSeqSystemSlot;

typedef struct NthSeqPromiseSlot {

} NthSeqPromiseSlot;


static struct {
    NthSequencerDesc desc;

    NthSeqSystemSlot *systems;
    NthSeqSize system_count;

    NthSeqPromiseSlot *promises;
    NthSeqSize promise_count;
} g_seq;


static inline nth_u64 nth_seq_total_due(NthUptimeNs now, NthDeltaNs period) {
    return now.ns / period.ns;
}


/* ================================================================================ */
/*  IMPLEMENTATION                                                                  */
/* ================================================================================ */


/* ================================================================================ */
/*  LIFE-CYCLE                                                                      */
/* ================================================================================ */

NTH_API NthResult nth_init_sequencer(const NthSequencerDesc *desc) {
    NTH_DASSERT(NTH_LIKELY(desc != NULL));
}
NTH_API void nth_term_sequencer(void);
