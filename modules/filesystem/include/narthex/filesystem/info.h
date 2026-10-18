#ifndef NTH_FILESYSTEM_INFO_H
#define NTH_FILESYSTEM_INFO_H

#include <narthex/nth_config.h>

#if !NTH_HAS_FILESYSTEM
#error "Narthex was built without the filesystem module"
#endif

#include <narthex/nth_result.h>
#include <narthex/nth_time.h>
#include <narthex/nth_types.h>
#include <narthex/nth_path.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u8 NthFsEntryType;
enum {
    NTH_FS_ENTRY_NONE       = 0,
    NTH_FS_ENTRY_FILE       = 1,
    NTH_FS_ENTRY_DIRECTORY  = 2,
    NTH_FS_ENTRY_SYMLINK    = 3,
    NTH_FS_ENTRY_OTHER      = 4, // device, socket, fifo, ...
};

typedef struct NthFsInfo {
    NthFsEntryType type;
    nth_u64 size;
    NthTime created;
    NthTime modified;
    NthTime accessed;
    nth_b8 readonly;
} NthFsInfo;


// follows symlinks
NTH_API NthFsEntryType nth_fs_type(const char *path);
NTH_API nth_b8 nth_fs_exists(const char *path);
NTH_API NthResult nth_fs_stat(const char *path, NthFsInfo *out);
// does not follow
NTH_API NthFsEntryType nth_fs_type_link(const char *path);
NTH_API nth_b8 nth_fs_exists_link(const char *path);
NTH_API NthResult nth_fs_stat_link(const char *path, NthFsInfo *out);

/* chase every symlink hop and normalizing '.'/'..' */
NTH_API NthResult nth_fs_resolve(const char *path, NthPath *out);


#ifdef __cplusplus
}
#endif

#endif /* NTH_FILESYSTEM_INFO_H */