#ifndef NTH_PATH_H
#define NTH_PATH_H

#include <narthex/mem/span.h>
#include <narthex/nth_result.h>
#include <narthex/utils/api.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct NthPath {
    nth_u8 *base;
    nth_u64 meta;
} NthPath;
typedef NthSpan NthPathView;


NTH_API NthResult nth_setup_path(NthPath *path);
NTH_API NthResult nth_setup_path_str(NthPath *path, const char *str);
NTH_API NthResult nth_setup_path_view(NthPath *path, NthPathView view);
NTH_API void nth_teardown_path(NthPath *path);

NTH_API void nth_path_system_config(NthPath *path); // XDG_CONFIG_HOME/<app> on Linux, %APPDATA%\<app> on Windows
NTH_API void nth_path_system_data(NthPath *path); // XDG_DATA_HOME/<app> on Linux, %LOCALAPPDATA%\<app> on Windows
NTH_API void nth_path_system_cwd(NthPath *path); // working directory at init time
NTH_API void nth_path_system_exe(NthPath *path); // directory of running executable

NTH_API void nth_path_set(NthPath *path, const char *str);
NTH_API void nth_path_setn(NthPath *path, const char *str, nth_usize len);
NTH_API void nth_path_append(NthPath *path, const char *str);
NTH_API void nth_path_appendn(NthPath *path, const char *str, nth_usize len);

NTH_API void nth_path_normalize(NthPath *path);
NTH_API void nth_path_clear(NthPath *path);
NTH_API void nth_path_copy(NthPath *dst, const NthPath *src);
// void nth_path_make_absolute(NthPath *path);
// void nth_path_make_relative(NthPath *path);
// void nth_path_set_extension(NthPath *path, const char *extension);
// void nth_path_remove_extension(NthPath *path);

NTH_API const char *nth_path_data(const NthPath *path);
NTH_API nth_usize nth_path_length(const NthPath *path);
NTH_API nth_b8 nth_path_empty(const NthPath *path);
NTH_API nth_b8 nth_path_is_absolute(const NthPath *path);
NTH_API nth_b8 nth_path_is_relative(const NthPath *path);

NTH_API NthPathView nth_path_filename(const NthPath *path);
NTH_API NthPathView nth_path_directory(const NthPath *path);
NTH_API NthPathView nth_path_extension(const NthPath *path);
NTH_API NthPathView nth_path_stem(const NthPath *path);

NTH_API nth_b8 nth_path_equal(const NthPath *a, const NthPath *b);
NTH_API nth_b8 nth_path_equal_view(const NthPath *path, NthPathView view);
NTH_API nth_b8 nth_path_view_equal(NthPathView a, NthPathView b);
NTH_API nth_b8 nth_path_view_equal_cstr(NthPathView view, const char *str);


#ifdef __cplusplus
}
#endif

#endif /* NTH_PATH_H */