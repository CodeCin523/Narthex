#ifndef NTH_UPTIME_H
#define NTH_UPTIME_H

#include <narthex/nth_types.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>
#include <narthex/utils/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthUptimeNs {nth_u64 ns;} NthUptimeNs;
typedef struct NthDeltaNs {nth_u64 ns;} NthDeltaNs;


#define NTH_NS_PER_SEC ((nth_u64)1000000000)

#define NTH_UPTIME_NS(raw) ((NthUptimeNs){ .ns = (raw) })
#define NTH_DELTA_NS(raw)  ((NthDeltaNs){ .ns = (raw) })


NTH_API NthUptimeNs nth_uptime_now(void);
NTH_API NthDeltaNs nth_uptime_elapsed(NthUptimeNs since);


static inline NthDeltaNs nth_delta_since(NthUptimeNs prev, NthUptimeNs now) {
    return (NthDeltaNs){ .ns = (now.ns > prev.ns) ? (now.ns - prev.ns) : 0 };
}
static inline NthDeltaNs nth_delta_clamp(NthDeltaNs dt, nth_u64 max_ns) {
    return (NthDeltaNs){ .ns = (dt.ns > max_ns) ? max_ns : dt.ns };
}
static inline nth_u64 nth_delta_sub_saturating(nth_u64 remaining_ns, NthDeltaNs dt) {
    return (remaining_ns > dt.ns) ? (remaining_ns - dt.ns) : 0;
}

static inline nth_f64 nth_delta_to_sec_f64(NthDeltaNs dt) {
    return (nth_f64)dt.ns / NTH_NS_PER_SEC;
}
static inline nth_f32 nth_delta_to_sec_f32(NthDeltaNs dt) {
    return (nth_f32)dt.ns / NTH_NS_PER_SEC;
}


#ifdef __cplusplus
}
#endif

#endif /* NTH_UPTIME_H */