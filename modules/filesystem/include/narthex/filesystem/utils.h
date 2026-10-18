#ifndef NTH_FILESYSTEM_UTILS_H
#define NTH_FILESYSTEM_UTILS_H

#include <narthex/nth_config.h>

#if !NTH_HAS_FILESYSTEM
#error "Narthex was built without the filesystem module"
#endif

#include <narthex/filesystem/info.h>

#ifdef __cplusplus
extern "C" {
#endif


NTH_API NthResult nth_fs_create_symlink(const char *target, const char *link_path, NthFsEntryType target_type);
NTH_API NthResult nth_fs_create_file(const char *path, nth_b8 overwrite);

NTH_API NthResult nth_fs_create_dir(const char *path);
NTH_API NthResult nth_fs_create_dir_all(const char *path);

NTH_API NthResult nth_fs_remove(const char *path);
NTH_API NthResult nth_fs_remove_all(const char *path);

NTH_API NthResult nth_fs_rename(const char *from, const char *to);


#ifdef __cplusplus
}
#endif

#endif /* NTH_FILESYSTEM_UTILS_H */