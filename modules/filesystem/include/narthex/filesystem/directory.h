#ifndef NTH_FILESYSTEM_DIRECTORY_H
#define NTH_FILESYSTEM_DIRECTORY_H

#include <narthex/nth_path.h>
#include <narthex/filesystem/info.h>
#include <narthex/utils/platform.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthFsDirEntry {
    NthPathView    name;
    NthFsEntryType type;
} NthFsDirEntry;

typedef struct NthFsDir {
#if NTH_PLATFORM_WINDOWS
    void *handle;
    nth_u8  find_data[608]; /* sized for WIN32_FIND_DATAA; verified by a _Static_assert in fs.c */
    nth_b8  has_pending;    /* FindFirstFileA already yielded an entry we haven't returned yet */
#else
    void   *handle; /* DIR* */
#endif
    nth_u64   meta;
} NthFsDir;


NTH_API NthResult nth_setup_fs_dir(NthFsDir *dir, const char *path);
NTH_API void nth_teardown_fs_dir(NthFsDir *dir);

NTH_API NthResult nth_fs_dir_next(NthFsDir *dir, NthFsDirEntry *out);


#ifdef __cplusplus
}
#endif

#endif /* NTH_FILESYSTEM_DIRECTORY_H */