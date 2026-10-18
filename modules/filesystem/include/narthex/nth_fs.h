#ifndef NTH_FS_H
#define NTH_FS_H

#include <narthex/nth_config.h>

#if !NTH_HAS_FILESYSTEM
#error "Narthex was built without the filesystem module"
#endif

#include <narthex/filesystem/info.h>
#include <narthex/filesystem/directory.h>
#include <narthex/filesystem/file.h>
#include <narthex/filesystem/system.h>
#include <narthex/filesystem/utils.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* NTH_FS_H */