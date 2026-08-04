#ifndef NTH_SRC_MUTEX_H
#define NTH_SRC_MUTEX_H

#include <narthex/utils/check.h>
#include <narthex/utils/platform.h>
#include <narthex/nth_types.h>

#if NTH_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <pthread.h>
#endif


/* Not recursive. Locking a mutex the calling thread already holds deadlocks. */
typedef struct NthMutex {
#if NTH_PLATFORM_WINDOWS
    SRWLOCK handle;
#else
    pthread_mutex_t handle;
#endif
} NthMutex;


#if NTH_PLATFORM_WINDOWS

static inline void nth_mutex_init(NthMutex *m) {
    InitializeSRWLock(&m->handle);
}

static inline void nth_mutex_lock(NthMutex *m) {
    AcquireSRWLockExclusive(&m->handle);
}

static inline void nth_mutex_unlock(NthMutex *m) {
    ReleaseSRWLockExclusive(&m->handle);
}

static inline void nth_mutex_destroy(NthMutex *m) {
    (void)m;
}

#else

static inline void nth_mutex_init(NthMutex *m) {
    const int rc = pthread_mutex_init(&m->handle, NULL);
    NTH_DASSERT(rc == 0);
    (void)rc;
}

static inline void nth_mutex_lock(NthMutex *m) {
    const int rc = pthread_mutex_lock(&m->handle);
    NTH_DASSERT(rc == 0);
    (void)rc;
}

static inline void nth_mutex_unlock(NthMutex *m) {
    const int rc = pthread_mutex_unlock(&m->handle);
    NTH_DASSERT(rc == 0);
    (void)rc;
}

static inline void nth_mutex_destroy(NthMutex *m) {
    const int rc = pthread_mutex_destroy(&m->handle);
    NTH_DASSERT(rc == 0);
    (void)rc;
}

#endif

#endif /* NTH_SRC_MUTEX_H */
