#ifndef NTH_FILESYSTEM_SYSTEM_H
#define NTH_FILESYSTEM_SYSTEM_H

#include <narthex/utils/api.h>
#include <narthex/nth_result.h>
#include <narthex/nth_path.h>

#ifdef __cplusplus
extern "C" {
#endif


NTH_API NthResult nth_fs_system_cwd(NthPath *out);
NTH_API NthResult nth_fs_system_exe(NthPath *out);
NTH_API NthResult nth_fs_system_temp(NthPath *out);
/* app_name may be NULL */
NTH_API NthResult nth_fs_system_config(const char *app_name, NthPath *out);
NTH_API NthResult nth_fs_system_data(const char *app_name, NthPath *out);


#ifdef __cplusplus
}
#endif

#endif /* NTH_FILESYSTEM_SYSTEM_H */