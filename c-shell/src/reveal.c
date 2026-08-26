#include "reveal.h"
#include "hop.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int cmp_names(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sa, *sb);
}

static int is_hidden(const char *name)
{
    return name[0] == '.';
}

static void print_directory(const char *path,
                            int show_all,
                            int recursive,
                            const char *prefix)
{
    DIR *dir = opendir(path);
    if (!dir)
        return;

    struct dirent *ent;
    char **names = NULL;
    size_t count = 0;
    size_t cap = 0;

    while ((ent = readdir(dir)) != NULL)
    {
        if (!show_all && is_hidden(ent->d_name))
            continue;

        if (count == cap)
        {
            cap = cap ? cap * 2 : 32;
            names = realloc(names, cap * sizeof(char *));
        }

        names[count++] = strdup(ent->d_name);
    }

    closedir(dir);

    if (count > 0 && names != NULL)
    {
        qsort(names, count, sizeof(char *), cmp_names);
    }

    for (size_t i = 0; i < count; i++)
    {
        char full[PATH_MAX * 2];
        snprintf(full, sizeof(full), "%s/%s", path, names[i]);

        struct stat st;
        int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        if (!recursive)
        {
            printf("%s%s\n", prefix, names[i]);
        }
        else
        {
            if (is_dir)
            {
                if (!strcmp(names[i], ".") || !strcmp(names[i], ".."))
                {
                    if (show_all)
                    {
                        printf("%s%s\n", prefix, names[i]);
                    }
                }
                else
                {
                    printf("%s%s/\n", prefix, names[i]);
                    char next_prefix[PATH_MAX * 2];
                    snprintf(next_prefix, sizeof(next_prefix), "%s%s/", prefix, names[i]);
                    print_directory(full, show_all, 1, next_prefix);
                }
            }
            else
            {
                printf("%s%s\n", prefix, names[i]);
            }
        }
    }

    for (size_t i = 0; i < count; i++)
        free(names[i]);

    free(names);
}

int reveal_builtin(int argc, char **argv)
{
    int show_all = 0;
    int recursive = 0;
    const char *raw_target = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-") == 0)
            {
                if (raw_target != NULL)
                {
                    puts("reveal: invalid syntax");
                    return 1;
                }
                raw_target = "-";
                continue;
            }

            if (argv[i][1] == '\0')
            {
                puts("reveal: invalid syntax");
                return 1;
            }

            for (size_t j = 1; argv[i][j]; j++)
            {
                if (argv[i][j] == 'a')
                {
                    show_all = 1;
                }
                else if (argv[i][j] == 't')
                {
                    recursive = 1;
                }
                else
                {
                    puts("reveal: invalid syntax");
                    return 1;
                }
            }
        }
        else
        {
            if (raw_target != NULL)
            {
                puts("reveal: invalid syntax");
                return 1;
            }

            raw_target = argv[i];
        }
    }

    char target[PATH_MAX * 2];

    if (raw_target == NULL || strcmp(raw_target, ".") == 0)
    {
        if (getcwd(target, sizeof(target)) == NULL)
            return 1;
    }
    else if (strcmp(raw_target, "~") == 0)
    {
        strncpy(target, hop_home(), sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }
    else if (strncmp(raw_target, "~/", 2) == 0)
    {
        snprintf(target, sizeof(target), "%s/%s", hop_home(), raw_target + 2);
    }
    else if (strcmp(raw_target, "-") == 0)
    {
        const char *prev = hop_previous();
        if (prev == NULL || prev[0] == '\0')
        {
            puts("reveal: no such directory");
            return 1;
        }

        strncpy(target, prev, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }
    else
    {
        strncpy(target, raw_target, sizeof(target) - 1);
        target[sizeof(target) - 1] = '\0';
    }

    struct stat st;

    if (stat(target, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        puts("reveal: no such directory");
        return 1;
    }

    print_directory(target,
                    show_all,
                    recursive,
                    "");

    return 0;
}
