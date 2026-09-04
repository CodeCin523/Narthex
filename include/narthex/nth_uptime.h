#ifndef NTH_UPTIME_H
#define NTH_UPTIME_H

#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthUptimeNs {nth_u64 ns;} NthUptimeNs;
typedef struct NthDeltaNs {nth_u64 ns;} NthDeltaNs;


#define NTH_NS_PER_SEC ((nth_u64)1000000000)

#define NTH_UPTIME_NS(raw) ((NthUptimeNs){ .ns = (raw) })
#define NTH_DELTA_NS(raw)  ((NthDeltaNs){ .ns = (raw) })

#define NTH_DELTA_SINCE(prev, now) \
    NTH_DELTA_NS((now).ns > (prev).ns ? (now).ns - (prev).ns : 0)

#define NTH_DELTA_CLAMP(dt, max_ns) \
    NTH_DELTA_NS((dt).ns > (max_ns) ? (max_ns) : (dt).ns)

#define NTH_DELTA_SUB_SATURATING(remaining_ns, dt) \
    ((remaining_ns) > (dt).ns ? (remaining_ns) - (dt).ns : 0)

#define NTH_DELTA_TO_SEC_F64(dt) ((nth_f64)(dt).ns / NTH_NS_PER_SEC)
#define NTH_DELTA_TO_SEC_F32(dt) ((nth_f32)NTH_DELTA_TO_SEC_F64(dt))


NTH_API NthUptimeNs nth_uptime_now(void);
NTH_API NthDeltaNs nth_uptime_elapsed(NthUptimeNs since);


#ifdef __cplusplus
}
#endif

#endif /* NTH_UPTIME_H */