#ifndef NTH_FILESYSTEM_FILE_H
#define NTH_FILESYSTEM_FILE_H

#include <narthex/filesystem/info.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef nth_u8 NthFsOpenMode;
enum {
    NTH_FS_OPEN_READ        = 0,
    NTH_FS_OPEN_WRITE       = 1,
    NTH_FS_OPEN_APPEND      = 2,
    NTH_FS_OPEN_READ_WRITE  = 3,
};

typedef nth_u8 NthFsSeekOrigin;
enum {
    NTH_FS_SEEK_START       = 0,
    NTH_FS_SEEK_CURRENT     = 1,
    NTH_FS_SEEK_END         = 2,
};

typedef struct NthFsFile {
    void *handle; /* FILE* */
    nth_u64 meta;
} NthFsFile;


NTH_API NthResult nth_setup_fs_file(NthFsFile *file, const char *path, NthFsOpenMode mode);
NTH_API void nth_teardown_fs_file(NthFsFile *file);

NTH_API NthResult nth_fs_file_read(NthFsFile *file, nth_u8 *buf, nth_usize buf_size, nth_usize *out_read);
NTH_API NthResult nth_fs_file_write(NthFsFile *file, const nth_u8 *data, nth_usize size, nth_usize *out_written);
NTH_API NthResult nth_fs_file_seek(NthFsFile *file, nth_i64 offset, NthFsSeekOrigin origin);
NTH_API NthResult nth_fs_file_tell(const NthFsFile *file, nth_u64 *out_pos);
NTH_API NthResult nth_fs_file_flush(NthFsFile *file);

NTH_API NthResult nth_fs_read_file(const char *path, nth_u8 *buf, nth_usize buf_size, nth_usize *out_size);
NTH_API NthResult nth_fs_write_file(const char *path, const nth_u8 *data, nth_usize size, nth_b8 overwrite);


#ifdef __cplusplus
}
#endif

#endif /* NTH_FILESYSTEM_FILE_H */