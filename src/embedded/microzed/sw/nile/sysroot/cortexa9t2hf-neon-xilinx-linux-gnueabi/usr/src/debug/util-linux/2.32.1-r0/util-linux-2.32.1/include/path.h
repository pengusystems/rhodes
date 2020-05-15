#ifndef UTIL_LINUX_PATH_H
#define UTIL_LINUX_PATH_H

#include <stdio.h>
#include <stdint.h>

/* Returns a pointer to a static buffer which may be destroyed by any later
path_* function call. NULL means error and errno will be set. */
/* Returns: 0 on success, sets errno on error. */
extern int path_set_prefix(const char *)
			__attribute__((warn_unused_result));

extern const char *path_get(const char *path, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));

extern FILE *path_fopen(const char *mode, int exit_on_err, const char *path, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
extern void path_read_str(char *result, size_t len, const char *path, ...)
			__attribute__ ((__format__ (__printf__, 3, 4)));
extern int path_write_str(const char *str, const char *path, ...)
			 __attribute__ ((__format__ (__printf__, 2, 3)));
extern int path_read_s32(const char *path, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));
extern uint64_t path_read_u64(const char *path, ...)
			__attribute__ ((__format__ (__printf__, 1, 2)));

extern int path_exist(const char *path, ...)
		      __attribute__ ((__format__ (__printf__, 1, 2)));

#ifdef HAVE_CPU_SET_T
# include "cpuset.h"

extern cpu_set_t *path_read_cpuset(int, const char *path, ...)
			      __attribute__ ((__format__ (__printf__, 2, 3)));
extern cpu_set_t *path_read_cpulist(int, const char *path, ...)
			       __attribute__ ((__format__ (__printf__, 2, 3)));

#endif /* HAVE_CPU_SET_T */
#endif /* UTIL_LINUX_PATH_H */
