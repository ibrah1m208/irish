#include "peek.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CHUNK_SIZE 4096

typedef struct {
    char *text;
    size_t len;
    int is_non_empty;
    int line_num;
} StreamLine;

/* Count non-empty lines in a seekable file by reading in chunks */
static int count_non_empty_lines_seekable(int fd)
{
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
        return 0;

    char buffer[CHUNK_SIZE];
    ssize_t nread;
    int count = 0;
    int line_has_content = 0;

    while ((nread = read(fd, buffer, sizeof(buffer))) > 0)
    {
        for (ssize_t i = 0; i < nread; i++)
        {
            char c = buffer[i];
            if (c == '\n')
            {
                if (line_has_content)
                {
                    count++;
                    line_has_content = 0;
                }
            }
            else if (c != '\r')
            {
                line_has_content = 1;
            }
        }
    }

    if (line_has_content)
    {
        count++;
    }

    return count;
}

/* Forward stream reader (for forward reading of files and stdin) */
static void peek_forward_stream(int fd, int flag_n, int *line_counter)
{
    int dup_fd = dup(fd);
    if (dup_fd < 0)
        return;

    FILE *fp = fdopen(dup_fd, "r");
    if (!fp)
    {
        close(dup_fd);
        return;
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t nread;

    while ((nread = getline(&line, &cap, fp)) != -1)
    {
        size_t len = (size_t)nread;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            len--;
        }

        if (len == 0)
        {
            putchar('\n');
        }
        else
        {
            if (flag_n)
            {
                printf("%d %.*s\n", (*line_counter)++, (int)len, line);
            }
            else
            {
                printf("%.*s\n", (int)len, line);
            }
        }
    }

    free(line);
    fclose(fp);
}

/* Reverse stream reader for non-seekable streams (stdin / pipes) */
static void peek_reverse_stream(int fd, int flag_n, int *line_counter)
{
    int dup_fd = dup(fd);
    if (dup_fd < 0)
        return;

    FILE *fp = fdopen(dup_fd, "r");
    if (!fp)
    {
        close(dup_fd);
        return;
    }

    StreamLine *lines = NULL;
    size_t count = 0;
    size_t cap = 0;

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t nread;

    while ((nread = getline(&line, &line_cap, fp)) != -1)
    {
        size_t len = (size_t)nread;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            len--;
        }

        if (count == cap)
        {
            cap = cap ? cap * 2 : 64;
            StreamLine *new_lines = realloc(lines, cap * sizeof(StreamLine));
            if (!new_lines)
            {
                break;
            }
            lines = new_lines;
        }

        lines[count].text = malloc(len + 1);
        if (lines[count].text)
        {
            memcpy(lines[count].text, line, len);
            lines[count].text[len] = '\0';
        }
        lines[count].len = len;
        lines[count].is_non_empty = (len > 0);
        if (len > 0)
        {
            lines[count].line_num = (*line_counter)++;
        }
        else
        {
            lines[count].line_num = 0;
        }
        count++;
    }

    free(line);
    fclose(fp);

    for (ssize_t i = (ssize_t)count - 1; i >= 0; i--)
    {
        if (lines[i].is_non_empty)
        {
            if (flag_n)
            {
                printf("%d %s\n", lines[i].line_num, lines[i].text ? lines[i].text : "");
            }
            else
            {
                printf("%s\n", lines[i].text ? lines[i].text : "");
            }
        }
        else
        {
            putchar('\n');
        }
        free(lines[i].text);
    }

    free(lines);
}

/* Reverse chunked reader using lseek for seekable regular files */
static void peek_reverse_seekable(int fd, off_t file_size, int flag_n, int *line_counter)
{
    if (file_size <= 0)
        return;

    int total_non_empty = 0;
    int current_line_num = 0;

    if (flag_n)
    {
        total_non_empty = count_non_empty_lines_seekable(fd);
        current_line_num = *line_counter + total_non_empty - 1;
    }

    off_t pos = file_size;
    char *tail = NULL;
    size_t tail_len = 0;

    char chunk[CHUNK_SIZE];

    while (pos > 0)
    {
        size_t read_len = (pos >= (off_t)sizeof(chunk)) ? sizeof(chunk) : (size_t)pos;
        pos -= (off_t)read_len;

        if (lseek(fd, pos, SEEK_SET) == (off_t)-1)
            break;

        ssize_t nread = read(fd, chunk, read_len);
        if (nread <= 0)
            break;

        size_t total_len = (size_t)nread + tail_len;
        char *combined = malloc(total_len);
        if (!combined)
            break;

        memcpy(combined, chunk, nread);
        if (tail_len > 0)
        {
            memcpy(combined + nread, tail, tail_len);
        }

        free(tail);
        tail = NULL;
        tail_len = 0;

        size_t end_idx = total_len;
        if ((size_t)(pos + nread) == (size_t)file_size)
        {
            if (total_len > 0 && combined[total_len - 1] == '\n')
            {
                end_idx = total_len - 1;
            }
        }

        for (ssize_t i = (ssize_t)end_idx - 1; i >= 0; i--)
        {
            if (combined[i] == '\n')
            {
                size_t line_start = (size_t)(i + 1);
                size_t line_len = end_idx - line_start;
                const char *line_ptr = combined + line_start;

                if (line_len > 0 && line_ptr[line_len - 1] == '\r')
                {
                    line_len--;
                }

                if (line_len == 0)
                {
                    putchar('\n');
                }
                else
                {
                    if (flag_n)
                    {
                        printf("%d %.*s\n", current_line_num--, (int)line_len, line_ptr);
                    }
                    else
                    {
                        printf("%.*s\n", (int)line_len, line_ptr);
                    }
                }

                end_idx = (size_t)i;
            }
        }

        if (end_idx > 0)
        {
            tail_len = end_idx;
            tail = malloc(tail_len);
            if (tail)
            {
                memcpy(tail, combined, tail_len);
            }
            else
            {
                tail_len = 0;
            }
        }

        free(combined);
    }

    if (tail_len > 0 || (pos == 0 && tail != NULL))
    {
        if (tail_len > 0 && tail[tail_len - 1] == '\r')
        {
            tail_len--;
        }

        if (tail_len == 0)
        {
            putchar('\n');
        }
        else
        {
            if (flag_n)
            {
                printf("%d %.*s\n", current_line_num--, (int)tail_len, tail);
            }
            else
            {
                printf("%.*s\n", (int)tail_len, tail);
            }
        }
    }
    else if (pos == 0 && file_size > 0 && tail_len == 0)
    {
        putchar('\n');
    }

    free(tail);

    if (flag_n)
    {
        *line_counter += total_non_empty;
    }
}

int peek_builtin(int argc, char **argv)
{
    int flag_n = 0;
    int flag_r = 0;
    const char **files = malloc((argc + 1) * sizeof(char *));
    if (!files)
    {
        return 1;
    }
    int file_count = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-") == 0)
            {
                files[file_count++] = "-";
                continue;
            }

            if (argv[i][1] == '\0')
            {
                puts("peek: invalid syntax");
                free(files);
                return 1;
            }

            for (size_t j = 1; argv[i][j]; j++)
            {
                if (argv[i][j] == 'n')
                {
                    flag_n = 1;
                }
                else if (argv[i][j] == 'r')
                {
                    flag_r = 1;
                }
                else
                {
                    puts("peek: invalid syntax");
                    free(files);
                    return 1;
                }
            }
        }
        else
        {
            files[file_count++] = argv[i];
        }
    }

    if (file_count == 0)
    {
        files[0] = "-";
        file_count = 1;
    }

    int global_line_counter = 1;
    int exit_status = 0;

    for (int i = 0; i < file_count; i++)
    {
        const char *filename = files[i];

        if (strcmp(filename, "-") == 0)
        {
            if (flag_r)
            {
                peek_reverse_stream(STDIN_FILENO, flag_n, &global_line_counter);
            }
            else
            {
                peek_forward_stream(STDIN_FILENO, flag_n, &global_line_counter);
            }
        }
        else
        {
            struct stat st;
            if (stat(filename, &st) != 0)
            {
                puts("peek: no such file or directory");
                exit_status = 1;
                continue;
            }

            if (S_ISDIR(st.st_mode))
            {
                puts("peek: is a directory");
                exit_status = 1;
                continue;
            }

            int fd = open(filename, O_RDONLY);
            if (fd < 0)
            {
                puts("peek: no such file or directory");
                exit_status = 1;
                continue;
            }

            if (flag_r)
            {
                peek_reverse_seekable(fd, st.st_size, flag_n, &global_line_counter);
            }
            else
            {
                peek_forward_stream(fd, flag_n, &global_line_counter);
            }

            close(fd);
        }
    }

    free(files);
    return exit_status;
}
