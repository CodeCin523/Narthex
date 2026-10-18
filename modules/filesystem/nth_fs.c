/* modules/filesystem/fs.c */
#include <narthex/nth_fs.h>

#include <narthex/utils/check.h>
#include <narthex/utils/compiler.h>
#include <narthex/utils/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#if NTH_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


#define FS_DIR_MAGIC  ((nth_u64)0x000000004E544644ULL) /* 'NTFD' */
#define FS_FILE_MAGIC ((nth_u64)0x000000004E544646ULL) /* 'NTFF' */

#if NTH_PLATFORM_WINDOWS
#define FS_SEP '\\'
#define FS_IS_SEP(c) ((c) == '/' || (c) == '\\')
#else
#define FS_SEP '/'
#define FS_IS_SEP(c) ((c) == '/')
#endif

#define NS_PER_SEC ((nth_i64)1000000000)


/* ================================================================================ */
/*  ERROR MAPPING                                                                   */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS
static NthResult win32_to_result(DWORD err) {
    switch(err) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:   return NTH_RESULT_NOT_FOUND;
        case ERROR_ACCESS_DENIED:    return NTH_RESULT_PERMISSION_DENIED;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:      return NTH_RESULT_ALREADY_EXISTS;
        case ERROR_DIR_NOT_EMPTY:    return NTH_RESULT_INVALID_STATE; /* closest existing fit */
        case ERROR_NOT_SAME_DEVICE:  return NTH_RESULT_NOT_SUPPORTED;
        case ERROR_DISK_FULL:        return NTH_RESULT_OUT_OF_MEMORY; /* closest existing fit; no DISK_FULL code today */
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_NAME:     return NTH_RESULT_INVALID_ARGUMENT;
        default:                     return NTH_RESULT_IO_ERROR;
    }
}

static NthTime filetime_to_nthtime(FILETIME ft) {
    const nth_i64 EPOCH_DIFF_100NS = 116444736000000000LL;
    nth_i64 ticks = ((nth_i64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (NthTime){ .ns = (ticks - EPOCH_DIFF_100NS) * 100 };
}
#else
static NthResult errno_to_result(int err) {
    switch(err) {
        case ENOENT:       return NTH_RESULT_NOT_FOUND;
        case EACCES:
        case EPERM:        return NTH_RESULT_PERMISSION_DENIED;
        case EEXIST:       return NTH_RESULT_ALREADY_EXISTS;
        case ENOTEMPTY:    return NTH_RESULT_INVALID_STATE;  /* closest existing fit */
        case EXDEV:        return NTH_RESULT_NOT_SUPPORTED;  /* cross-device rename */
        case ENOSPC:
        case EDQUOT:       return NTH_RESULT_OUT_OF_MEMORY;  /* closest existing fit; no DISK_FULL code today */
        case ENOTDIR:
        case EISDIR:
        case EINVAL:       return NTH_RESULT_INVALID_ARGUMENT;
        default:           return NTH_RESULT_IO_ERROR;
    }
}
#endif


/* ================================================================================ */
/*  QUERY                                                                           */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS

static NthResult fs_stat_impl(const char *path, nth_b8 follow, NthFsInfo *out) {
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS; /* lets us open a directory handle too */
    if(!follow)
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;

    HANDLE h = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, flags, NULL);
    if(h == INVALID_HANDLE_VALUE)
        return win32_to_result(GetLastError());

    BY_HANDLE_FILE_INFORMATION info;
    if(!GetFileInformationByHandle(h, &info)) {
        NthResult r = win32_to_result(GetLastError());
        CloseHandle(h);
        return r;
    }
    CloseHandle(h);

    if(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        out->type = NTH_FS_ENTRY_SYMLINK; /* junctions/mount points fold into this too; see note */
    else if(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        out->type = NTH_FS_ENTRY_DIRECTORY;
    else
        out->type = NTH_FS_ENTRY_FILE;

    out->size     = ((nth_u64)info.nFileSizeHigh << 32) | info.nFileSizeLow;
    out->created  = filetime_to_nthtime(info.ftCreationTime);
    out->modified = filetime_to_nthtime(info.ftLastWriteTime);
    out->accessed = filetime_to_nthtime(info.ftLastAccessTime);
    out->readonly = (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? NTH_TRUE : NTH_FALSE;

    return NTH_RESULT_OK;
}

#else /* POSIX */

static NthResult fs_stat_impl(const char *path, nth_b8 follow, NthFsInfo *out) {
#if defined(__linux__) && defined(STATX_BASIC_STATS)
    struct statx stx;
    int flags = follow ? 0 : AT_SYMLINK_NOFOLLOW;
    if(statx(AT_FDCWD, path, flags, STATX_BASIC_STATS | STATX_BTIME, &stx) != 0)
        return errno_to_result(errno);

    if(S_ISDIR(stx.stx_mode))      out->type = NTH_FS_ENTRY_DIRECTORY;
    else if(S_ISLNK(stx.stx_mode)) out->type = NTH_FS_ENTRY_SYMLINK;
    else if(S_ISREG(stx.stx_mode)) out->type = NTH_FS_ENTRY_FILE;
    else                           out->type = NTH_FS_ENTRY_OTHER;

    out->size     = stx.stx_size;
    out->modified = (NthTime){ .ns = (nth_i64)stx.stx_mtime.tv_sec * NS_PER_SEC + stx.stx_mtime.tv_nsec };
    out->accessed = (NthTime){ .ns = (nth_i64)stx.stx_atime.tv_sec * NS_PER_SEC + stx.stx_atime.tv_nsec };
    out->created  = (stx.stx_mask & STATX_BTIME)
                        ? (NthTime){ .ns = (nth_i64)stx.stx_btime.tv_sec * NS_PER_SEC + stx.stx_btime.tv_nsec }
                        : (NthTime){ 0 }; /* kernel too old, or the filesystem doesn't record it */

    out->readonly = !(stx.stx_mode & S_IWUSR);
    return NTH_RESULT_OK;
#else
    struct stat st;
    int r = follow ? stat(path, &st) : lstat(path, &st);
    if(r != 0)
        return errno_to_result(errno);

    if(S_ISDIR(st.st_mode))      out->type = NTH_FS_ENTRY_DIRECTORY;
    else if(S_ISLNK(st.st_mode)) out->type = NTH_FS_ENTRY_SYMLINK;
    else if(S_ISREG(st.st_mode)) out->type = NTH_FS_ENTRY_FILE;
    else                         out->type = NTH_FS_ENTRY_OTHER;

    out->size     = (nth_u64)st.st_size;
    out->modified = (NthTime){ .ns = (nth_i64)st.st_mtim.tv_sec * NS_PER_SEC + st.st_mtim.tv_nsec };
    out->accessed = (NthTime){ .ns = (nth_i64)st.st_atim.tv_sec * NS_PER_SEC + st.st_atim.tv_nsec };
    out->created  = (NthTime){ 0 }; /* plain stat() has no birth time on Linux without statx */
    out->readonly = !(st.st_mode & S_IWUSR);
    return NTH_RESULT_OK;
#endif
}

#endif


NthFsEntryType nth_fs_type(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NthFsInfo info;
    return (fs_stat_impl(path, NTH_TRUE, &info) == NTH_RESULT_OK) ? info.type : NTH_FS_ENTRY_NONE;
}
NthFsEntryType nth_fs_type_link(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NthFsInfo info;
    return (fs_stat_impl(path, NTH_FALSE, &info) == NTH_RESULT_OK) ? info.type : NTH_FS_ENTRY_NONE;
}

nth_b8 nth_fs_exists(const char *path)      { return nth_fs_type(path) != NTH_FS_ENTRY_NONE; }
nth_b8 nth_fs_exists_link(const char *path) { return nth_fs_type_link(path) != NTH_FS_ENTRY_NONE; }

NthResult nth_fs_stat(const char *path, NthFsInfo *out) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    return fs_stat_impl(path, NTH_TRUE, out);
}
NthResult nth_fs_stat_link(const char *path, NthFsInfo *out) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    return fs_stat_impl(path, NTH_FALSE, out);
}


/* ================================================================================ */
/*  RESOLVE                                                                         */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS

NthResult nth_fs_resolve(const char *path, NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    HANDLE h = CreateFileA(path, FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if(h == INVALID_HANDLE_VALUE)
        return win32_to_result(GetLastError());

    DWORD cap = MAX_PATH;
    char *buf = malloc(cap);
    if(buf == NULL) { CloseHandle(h); return NTH_RESULT_OUT_OF_MEMORY; }

    DWORD needed = GetFinalPathNameByHandleA(h, buf, cap, FILE_NAME_NORMALIZED);
    if(needed > cap) {
        char *grown = realloc(buf, needed);
        if(grown == NULL) { free(buf); CloseHandle(h); return NTH_RESULT_OUT_OF_MEMORY; }
        buf = grown;
        cap = needed;
        needed = GetFinalPathNameByHandleA(h, buf, cap, FILE_NAME_NORMALIZED);
    }
    CloseHandle(h);

    if(needed == 0) { free(buf); return NTH_RESULT_PLATFORM_ERROR; }

    /* GetFinalPathNameByHandleA commonly prepends the \\?\ long-path marker; strip it
       so output stays consistent with POSIX's unprefixed style. */
    const char *result = buf;
    if(needed >= 4 && strncmp(buf, "\\\\?\\", 4) == 0)
        result = buf + 4;

    nth_path_set(out, result);

    free(buf);
    return NTH_RESULT_OK;
}

#else

NthResult nth_fs_resolve(const char *path, NthPath *out) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(nth_path_is_alive(out)));

    char *resolved = realpath(path, NULL); /* glibc/BSD extension: mallocs the right size for us */
    if(resolved == NULL)
        return errno_to_result(errno);

    nth_path_set(out, resolved);
    free(resolved);
    return NTH_RESULT_OK;
}

#endif


/* ================================================================================ */
/*  DIRECTORY ITERATION                                                            */
/* ================================================================================ */

static nth_b8 fs_is_dot_entry(const char *name) {
    return (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')));
}

#if NTH_PLATFORM_WINDOWS

_Static_assert(sizeof(WIN32_FIND_DATAA) <= 608, "NthFsDir.find_data too small for WIN32_FIND_DATAA");

static NthFsEntryType fs_type_from_attrs(DWORD attrs) {
    if(attrs & FILE_ATTRIBUTE_REPARSE_POINT) return NTH_FS_ENTRY_SYMLINK;
    if(attrs & FILE_ATTRIBUTE_DIRECTORY)     return NTH_FS_ENTRY_DIRECTORY;
    return NTH_FS_ENTRY_FILE;
}

NthResult nth_setup_fs_dir(NthFsDir *dir, const char *path) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    NthPath pattern;
    NthResult r = nth_setup_path_cstr(&pattern, path);
    if(r != NTH_RESULT_OK) return r;
    nth_path_append(&pattern, "*");

    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)dir->find_data;
    HANDLE h = FindFirstFileA(nth_path_data(&pattern), fd);
    nth_teardown_path(&pattern);

    if(h == INVALID_HANDLE_VALUE)
        return win32_to_result(GetLastError());

    dir->handle      = h;
    dir->has_pending = NTH_TRUE;
    dir->meta        = FS_DIR_MAGIC;
    return NTH_RESULT_OK;
}

void nth_teardown_fs_dir(NthFsDir *dir) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_CHECK(NTH_LIKELY(dir->meta == FS_DIR_MAGIC), (void)0);

    FindClose((HANDLE)dir->handle);
    dir->meta = 0;
}

NthResult nth_fs_dir_next(NthFsDir *dir, NthFsDirEntry *out) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(dir->meta == FS_DIR_MAGIC));

    WIN32_FIND_DATAA *fd = (WIN32_FIND_DATAA *)dir->find_data;

    for(;;) {
        if(!dir->has_pending) {
            if(!FindNextFileA((HANDLE)dir->handle, fd)) {
                DWORD err = GetLastError();
                return (err == ERROR_NO_MORE_FILES) ? NTH_RESULT_NO_MORE_ITEMS : win32_to_result(err);
            }
        }
        dir->has_pending = NTH_FALSE;

        if(fs_is_dot_entry(fd->cFileName))
            continue;

        out->name.ptr  = (nth_u8 *)fd->cFileName;
        out->name.size = strlen(fd->cFileName);
        out->type      = fs_type_from_attrs(fd->dwFileAttributes);
        return NTH_RESULT_OK;
    }
}

#else /* POSIX */

NthResult nth_setup_fs_dir(NthFsDir *dir, const char *path) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    DIR *d = opendir(path);
    if(d == NULL)
        return errno_to_result(errno);

    dir->handle = d;
    dir->meta   = FS_DIR_MAGIC;
    return NTH_RESULT_OK;
}

void nth_teardown_fs_dir(NthFsDir *dir) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_CHECK(NTH_LIKELY(dir->meta == FS_DIR_MAGIC), (void)0);

    closedir((DIR *)dir->handle);
    dir->meta = 0;
}

NthResult nth_fs_dir_next(NthFsDir *dir, NthFsDirEntry *out) {
    NTH_DASSERT(NTH_LIKELY(dir != NULL));
    NTH_DASSERT(NTH_LIKELY(out != NULL));
    NTH_ASSERT(NTH_LIKELY(dir->meta == FS_DIR_MAGIC));

    for(;;) {
        errno = 0;
        struct dirent *de = readdir((DIR *)dir->handle);
        if(de == NULL)
            return (errno == 0) ? NTH_RESULT_NO_MORE_ITEMS : errno_to_result(errno);

        if(fs_is_dot_entry(de->d_name))
            continue;

        out->name.ptr  = (nth_u8 *)de->d_name;
        out->name.size = strlen(de->d_name);
#ifdef DT_UNKNOWN
        switch(de->d_type) {
            case DT_DIR: out->type = NTH_FS_ENTRY_DIRECTORY; break;
            case DT_LNK: out->type = NTH_FS_ENTRY_SYMLINK;   break;
            case DT_REG: out->type = NTH_FS_ENTRY_FILE;      break;
            default:     out->type = NTH_FS_ENTRY_OTHER;     break;
        }
#else
        out->type = NTH_FS_ENTRY_OTHER;
#endif
        return NTH_RESULT_OK;
    }
}


#endif


/* ================================================================================ */
/*  FILE I/O — identical on both platforms, no #if needed                          */
/* ================================================================================ */

NthResult nth_setup_fs_file(NthFsFile *file, const char *path, NthFsOpenMode mode) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    FILE *f = NULL;

    if(mode == NTH_FS_OPEN_READ_WRITE) {
        f = fopen(path, "r+b");
        if(f == NULL && errno == ENOENT)
            f = fopen(path, "w+b");
    } else {
        const char *mode_str;
        switch(mode) {
            case NTH_FS_OPEN_READ:   mode_str = "rb"; break;
            case NTH_FS_OPEN_WRITE:  mode_str = "wb"; break;
            case NTH_FS_OPEN_APPEND: mode_str = "ab"; break;
            default: NTH_UNREACHABLE(); return NTH_RESULT_INVALID_ARGUMENT;
        }
        f = fopen(path, mode_str);
    }

    if(f == NULL)
        return (mode == NTH_FS_OPEN_READ || mode == NTH_FS_OPEN_READ_WRITE) ? NTH_RESULT_NOT_FOUND : NTH_RESULT_IO_ERROR;

    file->handle = f;
    file->meta   = FS_FILE_MAGIC;
    return NTH_RESULT_OK;
}

void nth_teardown_fs_file(NthFsFile *file) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_CHECK(NTH_LIKELY(file->meta == FS_FILE_MAGIC), (void)0);

    fclose((FILE *)file->handle);
    file->meta = 0;
}

NthResult nth_fs_file_read(NthFsFile *file, nth_u8 *buf, nth_usize buf_size, nth_usize *out_read) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_DASSERT(NTH_LIKELY(buf != NULL));
    NTH_DASSERT(NTH_LIKELY(out_read != NULL));
    NTH_ASSERT(NTH_LIKELY(file->meta == FS_FILE_MAGIC));

    FILE *f = (FILE *)file->handle;
    nth_usize n = fread(buf, 1, buf_size, f);
    *out_read = n;

    if(n < buf_size && ferror(f)) /* resolves fread()'s own EOF-vs-error ambiguity */
        return NTH_RESULT_IO_ERROR;
    return NTH_RESULT_OK;
}

NthResult nth_fs_file_write(NthFsFile *file, const nth_u8 *data, nth_usize size, nth_usize *out_written) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_DASSERT(NTH_LIKELY(data != NULL));
    NTH_ASSERT(NTH_LIKELY(file->meta == FS_FILE_MAGIC));

    FILE *f = (FILE *)file->handle;
    nth_usize n = fwrite(data, 1, size, f);
    if(out_written != NULL) *out_written = n;

    return (n < size) ? NTH_RESULT_IO_ERROR : NTH_RESULT_OK;
}

NthResult nth_fs_file_seek(NthFsFile *file, nth_i64 offset, NthFsSeekOrigin origin) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_ASSERT(NTH_LIKELY(file->meta == FS_FILE_MAGIC));

    int whence = (origin == NTH_FS_SEEK_START) ? SEEK_SET : (origin == NTH_FS_SEEK_CURRENT) ? SEEK_CUR : SEEK_END;

#if NTH_PLATFORM_WINDOWS
    int r = _fseeki64((FILE *)file->handle, offset, whence);
#else
    int r = fseeko((FILE *)file->handle, (off_t)offset, whence);
#endif
    return (r != 0) ? NTH_RESULT_IO_ERROR : NTH_RESULT_OK;
}

NthResult nth_fs_file_tell(const NthFsFile *file, nth_u64 *out_pos) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_DASSERT(NTH_LIKELY(out_pos != NULL));
    NTH_ASSERT(NTH_LIKELY(file->meta == FS_FILE_MAGIC));

#if NTH_PLATFORM_WINDOWS
    __int64 pos = _ftelli64((FILE *)file->handle);
#else
    off_t pos = ftello((FILE *)file->handle);
#endif
    if(pos < 0) return NTH_RESULT_IO_ERROR;
    *out_pos = (nth_u64)pos;
    return NTH_RESULT_OK;
}

NthResult nth_fs_file_flush(NthFsFile *file) {
    NTH_DASSERT(NTH_LIKELY(file != NULL));
    NTH_ASSERT(NTH_LIKELY(file->meta == FS_FILE_MAGIC));
    return (fflush((FILE *)file->handle) != 0) ? NTH_RESULT_IO_ERROR : NTH_RESULT_OK;
}

NthResult nth_fs_read_file(const char *path, nth_u8 *buf, nth_usize buf_size, nth_usize *out_size) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(out_size != NULL));

    NthFsInfo info;
    NthResult r = nth_fs_stat(path, &info);
    if(r != NTH_RESULT_OK) return r;

    *out_size = (nth_usize)info.size;
    if(info.size > buf_size)
        return NTH_RESULT_CAPACITY_FULL; /* real size already written above so the caller can resize */

    NthFsFile file;
    r = nth_setup_fs_file(&file, path, NTH_FS_OPEN_READ);
    if(r != NTH_RESULT_OK) return r;

    nth_usize read = 0;
    r = nth_fs_file_read(&file, buf, (nth_usize)info.size, &read);
    nth_teardown_fs_file(&file);

    *out_size = read;
    return r;
}

NthResult nth_fs_write_file(const char *path, const nth_u8 *data, nth_usize size, nth_b8 overwrite) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
    NTH_DASSERT(NTH_LIKELY(data != NULL));

    if(!overwrite && nth_fs_exists(path))
        return NTH_RESULT_ALREADY_EXISTS;

    NthFsFile file;
    NthResult r = nth_setup_fs_file(&file, path, NTH_FS_OPEN_WRITE);
    if(r != NTH_RESULT_OK) return r;

    r = nth_fs_file_write(&file, data, size, NULL);
    nth_teardown_fs_file(&file);
    return r;
}


/* ================================================================================ */
/*  CREATE / REMOVE / RENAME                                                       */
/* ================================================================================ */

#if NTH_PLATFORM_WINDOWS
NthResult nth_fs_create_symlink(const char *target, const char *link_path, NthFsEntryType target_type) {
    NTH_DASSERT(NTH_LIKELY(target != NULL));
    NTH_DASSERT(NTH_LIKELY(link_path != NULL));
    NTH_DASSERT(NTH_LIKELY(target_type == NTH_FS_ENTRY_FILE || target_type == NTH_FS_ENTRY_DIRECTORY));

    /* ALLOW_UNPRIVILEGED_CREATE: Win10 1703+ with Developer Mode on. Without it (older
       systems, or Developer Mode off), this needs admin/SeCreateSymbolicLinkPrivilege
       and fails with ERROR_PRIVILEGE_NOT_HELD -> PERMISSION_DENIED below — expected,
       not a bug, on a locked-down machine. */
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if(target_type == NTH_FS_ENTRY_DIRECTORY)
        flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

    if(CreateSymbolicLinkA(link_path, target, flags))
        return NTH_RESULT_OK;
    return win32_to_result(GetLastError());
}
#else
NthResult nth_fs_create_symlink(const char *target, const char *link_path, NthFsEntryType target_type) {
    NTH_DASSERT(NTH_LIKELY(target != NULL));
    NTH_DASSERT(NTH_LIKELY(link_path != NULL));
    NTH_DASSERT(NTH_LIKELY(target_type == NTH_FS_ENTRY_FILE || target_type == NTH_FS_ENTRY_DIRECTORY));
    (void)target_type; /* no POSIX distinction; parameter exists only for Windows parity */

    if(symlink(target, link_path) == 0)
        return NTH_RESULT_OK;
    return errno_to_result(errno);
}
#endif
NthResult nth_fs_create_file(const char *path, nth_b8 overwrite) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    if(!overwrite && nth_fs_exists(path))
        return NTH_RESULT_ALREADY_EXISTS;

    NthFsFile file;
    NthResult r = nth_setup_fs_file(&file, path, NTH_FS_OPEN_WRITE); /* truncate-or-create */
    if(r != NTH_RESULT_OK) return r;

    nth_teardown_fs_file(&file);
    return NTH_RESULT_OK;
}

NthResult nth_fs_create_dir(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));
#if NTH_PLATFORM_WINDOWS
    if(CreateDirectoryA(path, NULL)) return NTH_RESULT_OK;
    return win32_to_result(GetLastError());
#else
    if(mkdir(path, 0755) == 0) return NTH_RESULT_OK;
    return errno_to_result(errno);
#endif
}

NthResult nth_fs_create_dir_all(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    nth_usize len = strlen(path);
    char *buf = malloc(len + 1);
    if(buf == NULL) return NTH_RESULT_OUT_OF_MEMORY;
    memcpy(buf, path, len + 1);

    NthResult result = NTH_RESULT_OK;
    for(nth_usize i = 1; i <= len; i++) {
        if(i == len || FS_IS_SEP(buf[i])) {
            char saved = buf[i];
            buf[i] = '\0';

            if(buf[0] != '\0') {
                NthResult cr = nth_fs_create_dir(buf);
                if(cr != NTH_RESULT_OK && cr != NTH_RESULT_ALREADY_EXISTS) {
                    buf[i] = saved;
                    result = cr;
                    goto done;
                }
            }
            buf[i] = saved;
        }
    }
done:
    free(buf);
    return result;
}

NthResult nth_fs_remove(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));

#if NTH_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    if(attrs == INVALID_FILE_ATTRIBUTES)
        return win32_to_result(GetLastError());

    /* A directory-type symlink/junction carries FILE_ATTRIBUTE_DIRECTORY alongside
       FILE_ATTRIBUTE_REPARSE_POINT, and needs RemoveDirectoryA — DeleteFileA fails
       on it. This has to check the raw bit, not NthFsEntryType, which collapses
       both file- and directory-symlinks into one SYMLINK value. */
    if(attrs & FILE_ATTRIBUTE_DIRECTORY) {
        if(RemoveDirectoryA(path)) return NTH_RESULT_OK;
    } else {
        if(DeleteFileA(path)) return NTH_RESULT_OK;
    }
    return win32_to_result(GetLastError());
#else
    NthFsEntryType t = nth_fs_type_link(path);
    NTH_CHECK(t != NTH_FS_ENTRY_NONE, NTH_RESULT_NOT_FOUND);

    if(t == NTH_FS_ENTRY_DIRECTORY) {
        if(rmdir(path) == 0) return NTH_RESULT_OK;
    } else {
        if(unlink(path) == 0) return NTH_RESULT_OK;
    }
    return errno_to_result(errno);
#endif
}

static NthResult fs_remove_all_recursive(const char *path) {
    NthFsDir dir;
    NthResult r = nth_setup_fs_dir(&dir, path);
    if(r != NTH_RESULT_OK) return r;

    NthResult result = NTH_RESULT_OK;
    NthFsDirEntry entry;
    NthResult step;

    while((step = nth_fs_dir_next(&dir, &entry)) == NTH_RESULT_OK) {
        NthPath child;
        NthResult cr = nth_setup_path_cstr(&child, path);
        if(cr != NTH_RESULT_OK) { result = cr; break; }

        nth_path_appendn(&child, (const char *)entry.name.ptr, entry.name.size);

        cr = (entry.type == NTH_FS_ENTRY_DIRECTORY)
                 ? fs_remove_all_recursive(nth_path_data(&child))
                 : nth_fs_remove(nth_path_data(&child));

        nth_teardown_path(&child);
        if(cr != NTH_RESULT_OK) { result = cr; break; }
    }

    nth_teardown_fs_dir(&dir);

    if(result == NTH_RESULT_OK && step != NTH_RESULT_NO_MORE_ITEMS)
        result = step; /* a real iteration error, not just end-of-directory */

    if(result != NTH_RESULT_OK) return result;
    return nth_fs_remove(path);
}

NthResult nth_fs_remove_all(const char *path) {
    NTH_DASSERT(NTH_LIKELY(path != NULL));

    NthFsEntryType t = nth_fs_type_link(path);
    if(t == NTH_FS_ENTRY_SYMLINK) return NTH_RESULT_INVALID_ARGUMENT;
    if(t != NTH_FS_ENTRY_DIRECTORY) return NTH_RESULT_NOT_FOUND;

    return fs_remove_all_recursive(path);
}

NthResult nth_fs_rename(const char *from, const char *to) {
    NTH_DASSERT(NTH_LIKELY(from != NULL));
    NTH_DASSERT(NTH_LIKELY(to != NULL));

#if NTH_PLATFORM_WINDOWS
    /* MOVEFILE_REPLACE_EXISTING: plain CRT rename()/MoveFileA fail if `to` exists on
       Windows, unlike POSIX rename()'s silent atomic replace. This flag is what makes
       the two platforms actually behave the same. */
    if(MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING))
        return NTH_RESULT_OK;
    return win32_to_result(GetLastError());
#else
    if(rename(from, to) == 0) return NTH_RESULT_OK;
    return errno_to_result(errno);
#endif
}