#ifndef NTH_CORE_H
#define NTH_CORE_H

#include <narthex/nth_types.h>
#include <narthex/nth_result.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthLoggerDesc {

} NthLoggerDesc;

typedef struct NthCoreDesc {
    NthLoggerDesc logger;
} NthCoreDesc;


NthResult nth_init(const NthCoreDesc *desc);
void nth_term(void);


#ifdef __cplusplus
}
#endif

#endif /* NTH_CORE_H */