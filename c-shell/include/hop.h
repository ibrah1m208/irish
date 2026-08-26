#ifndef HOP_H
#define HOP_H

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void hop_init(const char *home); // initialize once when shell starts

int hop_builtin(int argc, char **argv);

const char *hop_home(void);
const char *hop_previous(void);

#endif