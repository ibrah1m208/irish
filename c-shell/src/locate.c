#include "locate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Check whether path exists, is not a directory, and is executable */
static int is_executable(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 0;

    return access(path, X_OK) == 0;
}

static void print_absolute(const char *path)
{
    char abs[PATH_MAX];

    if (realpath(path, abs) != NULL)
        puts(abs);
}

int locate_builtin(int argc, char **argv)
{
    if (argc < 2)
    {
        puts("locate: invalid syntax");
        return 1;
    }

    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return 1;

    const char *path_env = getenv("PATH");

    for (int i = 1; i < argc; i++)
    {
        int found = 0;

        /* ---------- Current directory ---------- */

        char candidate[PATH_MAX * 2];

        snprintf(candidate,
                 sizeof(candidate),
                 "%s/%s",
                 cwd,
                 argv[i]);

        if (is_executable(candidate))
        {
            print_absolute(candidate);
            found = 1;
        }

        /* ---------- PATH ---------- */

        if (path_env)
        {
            char *copy = strdup(path_env);

            if (!copy)
                continue;

            char *saveptr = NULL;

            char *dir = strtok_r(copy, ":", &saveptr);

            while (dir)
            {
                snprintf(candidate,
                         sizeof(candidate),
                         "%s/%s",
                         dir,
                         argv[i]);

                if (is_executable(candidate))
                {
                    print_absolute(candidate);
                    found = 1;
                }

                dir = strtok_r(NULL,
                               ":",
                               &saveptr);
            }

            free(copy);
        }

        if (!found)
            printf("locate: command not found (%s)\n",
                   argv[i]);
    }

    return 0;
}
