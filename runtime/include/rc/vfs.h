/* SPDX-License-Identifier: MIT */
/* Mapping of Windows-style guest paths onto the host file system.
 * The games use backslashes and inconsistent case ("Data_FMV\Intro.bik" for
 * "data_fmv/intro.bik"), so lookups are case-insensitive per path component. */
#ifndef RC_VFS_H
#define RC_VFS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve guest_path (relative, '\' or '/' separated; "." and empty components ignored,
 * ".." rejected) below the host directory root. On success writes the existing host path
 * to out and returns 0. Returns -1 if a component does not exist or out is too small.
 * If the last component does not exist, the path is still built (for creating files)
 * and 1 is returned. */
int rc_vfs_resolve(const char *root, const char *guest_path, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
#endif
