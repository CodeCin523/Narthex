#ifndef NTH_SEQUENCER_H
#define NTH_SEQUENCER_H

#include <narthex/sequencer/types.h>
#include <narthex/sequencer/core.h>

#ifdef __cplusplus
extern "C" {
#endif









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