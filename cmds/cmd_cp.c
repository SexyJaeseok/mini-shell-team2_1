/* 김시현 담당: cp
 * cp 와 mv 가 함께 쓰는 목적지 결정 코드(run_transfer)도 이 파일에 둔다. */
#define _POSIX_C_SOURCE 200809L

#include "../shell.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int copy_stream(FILE *src, FILE *dst) {
    char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dst) != n)
            return 1;
    }

    return ferror(src) || ferror(dst);
}

/* directory/name 형태의 경로를 새로 만든다. (free 는 호출한 쪽에서) */
static char *join_path(const char *directory, const char *name) {
    size_t length = strlen(directory);
    int slash = length > 0 && directory[length - 1] == '/';
    size_t size = length + (slash ? 0 : 1) + strlen(name) + 1;

    char *path = malloc(size);
    if (!path) {
        perror("path");
        return NULL;
    }
    snprintf(path, size, "%s%s%s", directory, slash ? "" : "/", name);
    return path;
}

/* cp는 두 열린 파일의 장치 번호와 inode를 비교한 뒤 대상 파일의 길이를 줄인다. */
static int copy_file(const char *source, const char *destination) {
    FILE *input = fopen(source, "rb");
    if (!input) {
        perror(source);
        return 1;
    }

    struct stat from, to;
    if (fstat(fileno(input), &from) < 0) {
        perror(source);
        fclose(input);
        return 1;
    }
    if (S_ISDIR(from.st_mode)) {
        fprintf(stderr, "cp: %s: source is a directory\n", source);
        fclose(input);
        return 1;
    }

    int fd = open(destination, O_WRONLY | O_CREAT, 0666);
    if (fd < 0) {
        perror(destination);
        fclose(input);
        return 1;
    }

    int result = 1;
    if (fstat(fd, &to) < 0) {
        perror(destination);
    } else if (from.st_dev == to.st_dev && from.st_ino == to.st_ino) {
        fprintf(stderr, "cp: same file\n");
    } else if (S_ISREG(to.st_mode) && ftruncate(fd, 0) < 0) {
        perror(destination);
    } else {
        FILE *output = fdopen(fd, "wb");
        if (!output) {
            perror(destination);
        } else {
            result = copy_stream(input, output);
            if (fclose(output) != 0)
                result = 1;
            if (result)
                fprintf(stderr, "cp: copy failed: %s\n", destination);
            fd = -1;
        }
    }

    if (fd >= 0)
        close(fd);
    fclose(input);
    return result;
}

/* 마지막 인자가 디렉터리이면 원본 이름을 그대로 붙여 목적지를 정한다. */
static char *destination_in(const char *directory, const char *source) {
    size_t end = strlen(source);
    while (end && source[end - 1] == '/')
        end--;
    if (!end) {
        fprintf(stderr, "Missing source name\n");
        return NULL;
    }
    size_t start = end;
    while (start && source[start - 1] != '/')
        start--;

    char *name = strndup(source + start, end - start);
    if (!name) {
        perror("source name");
        return NULL;
    }
    char *destination = join_path(directory, name);
    free(name);
    return destination;
}

/* cp(move == 0)와 mv(move == 1)가 공유한다. shell.h 에 선언되어 있다. */
int run_transfer(char *argv[], int move) {
    int first = argv[1] && !strcmp(argv[1], "--") ? 2 : 1;
    int count = first;
    while (argv[count])
        count++;

    if (count - first < 2) {
        fprintf(stderr, "Usage: %s source ... destination\n", argv[0]);
        return 1;
    }

    const char *target = argv[count - 1];
    struct stat info;
    int directory = stat(target, &info) == 0 && S_ISDIR(info.st_mode);
    if (count - first > 2 && !directory) {
        fprintf(stderr, "%s: destination must be a directory\n", argv[0]);
        return 1;
    }

    int result = 0;
    for (int i = first; i < count - 1; i++) {
        char *joined = NULL;
        if (directory) {
            joined = destination_in(target, argv[i]);
            if (!joined) {
                result = 1;
                continue;
            }
        }

        const char *destination = directory ? joined : target;
        if (move) {
            if (rename(argv[i], destination) == -1) {
                perror(argv[i]);
                result = 1;
            }
        } else if (copy_file(argv[i], destination)) {
            result = 1;
        }
        free(joined);
    }
    return result;
}

int cmd_cp(int argc, char *argv[]) {
    return run_transfer(argv, 0);
}
