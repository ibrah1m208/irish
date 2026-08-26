#include "hop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>

#define DB_FILE ".cshell_frecency"
#define MAX_ENTRY 1024

typedef struct {
    char path[PATH_MAX];
    unsigned long visits;
    time_t last_visit;
} Entry;

static char shell_home[PATH_MAX];
static char previous_dir[PATH_MAX];
static char db_path[PATH_MAX + 64];

static Entry entries[MAX_ENTRY];
static size_t entry_count = 0;

static void load_db(void)
{
    FILE *fp = fopen(db_path, "r");
    if (!fp)
        return;

    entry_count = 0;
    char line[PATH_MAX + 128];
    while (entry_count < MAX_ENTRY && fgets(line, sizeof(line), fp))
    {
        unsigned long visits = 0;
        long last_visit = 0;
        char path_buf[PATH_MAX];

        line[strcspn(line, "\r\n")] = '\0';
        if (sscanf(line, "%lu %ld %4095[^\n]", &visits, &last_visit, path_buf) == 3)
        {
            strncpy(entries[entry_count].path, path_buf, PATH_MAX - 1);
            entries[entry_count].path[PATH_MAX - 1] = '\0';
            entries[entry_count].visits = visits;
            entries[entry_count].last_visit = (time_t)last_visit;
            entry_count++;
        }
    }

    fclose(fp);
}

static void save_db(void)
{
    FILE *fp = fopen(db_path, "w");
    if (!fp)
        return;

    for (size_t i = 0; i < entry_count; i++)
    {
        fprintf(fp,
                "%lu %ld %s\n",
                entries[i].visits,
                (long)entries[i].last_visit,
                entries[i].path);
    }

    fclose(fp);
}

static void update_entry(const char *path)
{
    for (size_t i = 0; i < entry_count; i++)
    {
        if (strcmp(entries[i].path, path) == 0)
        {
            entries[i].visits++;
            entries[i].last_visit = time(NULL);
            save_db();
            return;
        }
    }

    if (entry_count >= MAX_ENTRY)
        return;

    strncpy(entries[entry_count].path, path, PATH_MAX - 1);
    entries[entry_count].path[PATH_MAX - 1] = '\0';
    entries[entry_count].visits = 1;
    entries[entry_count].last_visit = time(NULL);
    entry_count++;

    save_db();
}

static double score(const Entry *e)
{
    double age = difftime(time(NULL), e->last_visit) / 3600.0;
    if (age < 0.0)
        age = 0.0;

    return (double)e->visits / (1.0 + age);
}

static int exists_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

static const char *search_frecency(const char *name)
{
    double best = -1.0;
    int best_idx = -1;

    for (size_t i = 0; i < entry_count; i++)
    {
        if (!strstr(entries[i].path, name))
            continue;

        if (!exists_dir(entries[i].path))
            continue;

        double s = score(&entries[i]);
        if (s > best)
        {
            best = s;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0)
        return entries[best_idx].path;

    return NULL;
}

void hop_init(const char *home)
{
    if (home)
    {
        strncpy(shell_home, home, PATH_MAX - 1);
        shell_home[PATH_MAX - 1] = '\0';
    }
    else
    {
        if (getcwd(shell_home, sizeof(shell_home)) == NULL)
            strcpy(shell_home, "/");
    }

    previous_dir[0] = '\0';

    snprintf(db_path,
             sizeof(db_path),
             "%s/%s",
             shell_home,
             DB_FILE);

    load_db();
}

const char *hop_home(void)
{
    return shell_home;
}

const char *hop_previous(void)
{
    return previous_dir;
}

static int perform_cd(const char *target)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return -1;

    if (chdir(target) != 0)
        return -1;

    strncpy(previous_dir, cwd, PATH_MAX - 1);
    previous_dir[PATH_MAX - 1] = '\0';

    char new_cwd[PATH_MAX];
    if (getcwd(new_cwd, sizeof(new_cwd)) != NULL)
    {
        update_entry(new_cwd);
    }

    return 0;
}

int hop_builtin(int argc, char **argv)
{
    if (argc == 1)
    {
        if (perform_cd(shell_home) != 0)
            puts("hop: no such directory");

        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (!strcmp(arg, "~"))
        {
            if (perform_cd(shell_home) != 0)
                puts("hop: no such directory");

            continue;
        }

        if (strncmp(arg, "~/", 2) == 0)
        {
            char expanded[PATH_MAX * 2];
            snprintf(expanded, sizeof(expanded), "%s/%s", shell_home, arg + 2);
            if (exists_dir(expanded))
            {
                if (perform_cd(expanded) != 0)
                    puts("hop: no such directory");

                continue;
            }
        }

        if (!strcmp(arg, "."))
            continue;

        if (!strcmp(arg, ".."))
        {
            if (perform_cd("..") != 0)
                puts("hop: no such directory");

            continue;
        }

        if (!strcmp(arg, "-"))
        {
            if (!previous_dir[0])
            {
                /* Requirement 4: do nothing if there was no previous CWD */
                continue;
            }

            if (perform_cd(previous_dir) != 0)
            {
                puts("hop: no such directory");
            }

            continue;
        }

        if (exists_dir(arg))
        {
            if (perform_cd(arg) != 0)
                puts("hop: no such directory");

            continue;
        }

        const char *match = search_frecency(arg);
        if (match)
        {
            if (perform_cd(match) != 0)
                puts("hop: no such directory");

            continue;
        }

        puts("hop: no such directory");
    }

    return 0;
}