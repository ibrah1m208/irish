#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

/* ---- Buffer constants ---- */
#define HOSTNAME_MAX_LEN 256
#define DYNSTRING_DEFAULT_CAPACITY 16


#ifndef PATH_MAX                        // Apparently PATH_MAX isn't defined on every POSIX system (ex. gnu HURD)
#define PATH_MAX 4096                   // So here is a safe fallback justin case
#endif

#endif // SHELL_H