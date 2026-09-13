#include "spy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_MEM_TRACKED 2048

static const char *get_file_type(const char *path, mode_t mode) {
    if (S_ISREG(mode)) return "REG";
    if (S_ISDIR(mode)) return "DIR";
    if (S_ISCHR(mode)) return "CHR";
    if (S_ISBLK(mode)) return "BLK";
    if (S_ISFIFO(mode)) return "FIFO";
    if (S_ISSOCK(mode)) return "SOCK";
    if (S_ISLNK(mode)) return "LNK";

    if (path != NULL) {
        if (strncmp(path, "pipe:", 5) == 0) return "FIFO";
        if (strncmp(path, "socket:", 7) == 0) return "SOCK";
        if (strncmp(path, "anon_inode:", 11) == 0) return "REG";
    }

    return "UNKNOWN";
}

static int compare_ints(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

int spy_builtin(int argc, char *argv[]) {
    if (argc > 2) {
        printf("spy: invalid syntax\n");
        return -1;
    }

    long pid_val = 0;
    if (argc == 2) {
        char *endptr = NULL;
        pid_val = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || pid_val <= 0 || argv[1][0] == '-') {
            printf("spy: no such process\n");
            return -1;
        }
    } else {
        pid_val = (long)getpid();
    }

    char proc_dir[128];
    snprintf(proc_dir, sizeof(proc_dir), "/proc/%ld", pid_val);
    if (access(proc_dir, F_OK) != 0) {
        printf("spy: no such process\n");
        return -1;
    }

    printf("PID    FD    TYPE   PATH\n");

    char link_path[256];
    char target_path[4096];
    ssize_t len;

    // 1. cwd
    snprintf(link_path, sizeof(link_path), "/proc/%ld/cwd", pid_val);
    len = readlink(link_path, target_path, sizeof(target_path) - 1);
    if (len > 0) {
        target_path[len] = '\0';
        struct stat st;
        const char *t = "DIR";
        if (stat(link_path, &st) == 0) {
            t = get_file_type(target_path, st.st_mode);
        }
        printf("%-6ld %-7s %-7s %s\n", pid_val, "cwd", t, target_path);
    }

    // 2. txt (executable)
    char txt_path[4096] = {0};
    snprintf(link_path, sizeof(link_path), "/proc/%ld/exe", pid_val);
    len = readlink(link_path, txt_path, sizeof(txt_path) - 1);
    if (len > 0) {
        txt_path[len] = '\0';
        struct stat st;
        const char *t = "REG";
        if (stat(link_path, &st) == 0) {
            t = get_file_type(txt_path, st.st_mode);
        }
        printf("%-6ld %-7s %-7s %s\n", pid_val, "txt", t, txt_path);
    }

    // 3. mem (memory-mapped files from /proc/<pid>/maps)
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path), "/proc/%ld/maps", pid_val);
    FILE *maps_file = fopen(maps_path, "r");
    if (maps_file != NULL) {
        char *seen_paths[MAX_MEM_TRACKED];
        size_t seen_count = 0;

        char line[4096];
        while (fgets(line, sizeof(line), maps_file) != NULL) {
            char *slash = strchr(line, '/');
            if (slash == NULL) {
                continue;
            }

            size_t path_len = strlen(slash);
            while (path_len > 0 && (slash[path_len - 1] == '\n' ||
                                    slash[path_len - 1] == '\r' ||
                                    slash[path_len - 1] == ' ' ||
                                    slash[path_len - 1] == '\t')) {
                slash[--path_len] = '\0';
            }

            if (path_len == 0) {
                continue;
            }

            if (txt_path[0] != '\0' && strcmp(slash, txt_path) == 0) {
                continue;
            }

            int already_seen = 0;
            for (size_t s = 0; s < seen_count; s++) {
                if (strcmp(seen_paths[s], slash) == 0) {
                    already_seen = 1;
                    break;
                }
            }
            if (already_seen) {
                continue;
            }

            if (seen_count < MAX_MEM_TRACKED) {
                seen_paths[seen_count++] = strdup(slash);
            }

            struct stat st;
            const char *t = "REG";
            if (stat(slash, &st) == 0) {
                t = get_file_type(slash, st.st_mode);
            }
            printf("%-6ld %-7s %-7s %s\n", pid_val, "mem", t, slash);
        }

        fclose(maps_file);

        for (size_t s = 0; s < seen_count; s++) {
            free(seen_paths[s]);
        }
    }

    // 4. Numeric file descriptors from /proc/<pid>/fd
    char fd_dir_path[256];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%ld/fd", pid_val);
    DIR *dir = opendir(fd_dir_path);
    if (dir != NULL) {
        int fd_list[1024];
        size_t fd_count = 0;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char *endptr = NULL;
            long val = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0' && val >= 0) {
                if (fd_count < 1024) {
                    fd_list[fd_count++] = (int)val;
                }
            }
        }
        closedir(dir);

        qsort(fd_list, fd_count, sizeof(int), compare_ints);

        for (size_t i = 0; i < fd_count; i++) {
            snprintf(link_path, sizeof(link_path), "/proc/%ld/fd/%d", pid_val, fd_list[i]);
            len = readlink(link_path, target_path, sizeof(target_path) - 1);
            if (len > 0) {
                target_path[len] = '\0';
                struct stat st;
                const char *t = "UNKNOWN";
                if (stat(link_path, &st) == 0) {
                    t = get_file_type(target_path, st.st_mode);
                } else {
                    t = get_file_type(target_path, 0);
                }
                char fd_str[16];
                snprintf(fd_str, sizeof(fd_str), "%d", fd_list[i]);
                printf("%-6ld %-7s %-7s %s\n", pid_val, fd_str, t, target_path);
            }
        }
    }

    fflush(stdout);
    return 0;
}
