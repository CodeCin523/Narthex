#ifndef NTH_LOG_H
#define NTH_LOG_H

#include <narthex/utils/build.h>
#include <narthex/nth_types.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u8 NthLogLevel;
enum {
    NTH_LOG_LEVEL_FATAL = 0,
    NTH_LOG_LEVEL_ERROR = 1,
    NTH_LOG_LEVEL_WARN  = 2,
    NTH_LOG_LEVEL_INFO  = 3,
    NTH_LOG_LEVEL_DEBUG = 4
};


NTH_API void nth_log(NthLogLevel level, const char *msg);
NTH_API void nth_logn(NthLogLevel level, const char *msg, nth_usize len);
NTH_API void nth_logf(NthLogLevel level, const char *fmt, ...);

NTH_API void nth_flush(void);


#ifndef NTH_LOG_FATAL_ENABLED
#define NTH_LOG_FATAL_ENABLED 1
#endif
#ifndef NTH_LOG_ERROR_ENABLED
#define NTH_LOG_ERROR_ENABLED 1
#endif
#ifndef NTH_LOG_WARN_ENABLED
#define NTH_LOG_WARN_ENABLED  1
#endif
#ifndef NTH_LOG_INFO_ENABLED
#define NTH_LOG_INFO_ENABLED  1
#endif
#ifndef NTH_LOG_DEBUG_ENABLED
#define NTH_LOG_DEBUG_ENABLED NTH_DEBUG
#endif


#if NTH_LOG_FATAL_ENABLED
#define NTH_LOG_FATAL(msg) (nth_log(NTH_LOG_LEVEL_FATAL, (msg)))
#define NTH_LOGF_FATAL(fmt, ...) (nth_logf(NTH_LOG_LEVEL_FATAL, (fmt), ##__VA_ARGS__))
#else
#define NTH_LOG_FATAL(msg) ((void)0)
#define NTH_LOGF_FATAL(fmt, ...) ((void)0)
#endif

#if NTH_LOG_ERROR_ENABLED
#define NTH_LOG_ERROR(msg) (nth_log(NTH_LOG_LEVEL_ERROR, (msg)))
#define NTH_LOGF_ERROR(fmt, ...) (nth_logf(NTH_LOG_LEVEL_ERROR, (fmt), ##__VA_ARGS__))
#else
#define NTH_LOG_ERROR(msg) ((void)0)
#define NTH_LOGF_ERROR(fmt, ...) ((void)0)
#endif

#if NTH_LOG_WARN_ENABLED
#define NTH_LOG_WARN(msg) (nth_log(NTH_LOG_LEVEL_WARN, (msg)))
#define NTH_LOGF_WARN(fmt, ...) (nth_logf(NTH_LOG_LEVEL_WARN, (fmt), ##__VA_ARGS__))
#else
#define NTH_LOG_WARN(msg) ((void)0)
#define NTH_LOGF_WARN(fmt, ...) ((void)0)
#endif

#if NTH_LOG_INFO_ENABLED
#define NTH_LOG_INFO(msg) (nth_log(NTH_LOG_LEVEL_INFO, (msg)))
#define NTH_LOGF_INFO(fmt, ...) (nth_logf(NTH_LOG_LEVEL_INFO, (fmt), ##__VA_ARGS__))
#else
#define NTH_LOG_INFO(msg) ((void)0)
#define NTH_LOGF_INFO(fmt, ...) ((void)0)
#endif

#if NTH_LOG_DEBUG_ENABLED
#define NTH_LOG_DEBUG(msg) (nth_log(NTH_LOG_LEVEL_DEBUG, (msg)))
#define NTH_LOGF_DEBUG(fmt, ...) (nth_logf(NTH_LOG_LEVEL_DEBUG, (fmt), ##__VA_ARGS__))
#else
#define NTH_LOG_DEBUG(msg) ((void)0)
#define NTH_LOGF_DEBUG(fmt, ...) ((void)0)
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_LOG_H */