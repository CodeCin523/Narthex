#ifndef NTH_SRC_LOG_INTERNAL_H
#define NTH_SRC_LOG_INTERNAL_H

#include <narthex/narthex.h>
#include <narthex/nth_result.h>
#include <narthex/nth_types.h>


/* Neither may run concurrently with nth_log, nth_logf or nth_flush. */
NthResult nth_init_log(const NthLoggerDesc *desc);
void nth_term_log(void);


#endif /* NTH_SRC_LOG_INTERNAL_H */
