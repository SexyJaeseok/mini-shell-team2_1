/* rm [-r] [-f] 파일...
 *  -r : 디렉토리와 그 안의 내용을 모두 지운다
 *  -f : 없는 파일은 오류로 보지 않고, 오류 메시지도 출력하지 않는다 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../shell.h"

/* 디렉토리 안을 모두 지운 뒤 디렉토리 자신을 지운다.
 * lstat 을 쓰므로 심볼릭 링크는 따라가지 않고 링크 자체만 지운다. */
static int remove_tree(const char *path, int force)
{
    struct stat st;
    DIR *dir;
    struct dirent *e;
    char child[4096];
    int status = 0;

    if (lstat(path, &st) == -1) {
        if (force && errno == ENOENT)
            return 0;
        fprintf(stderr, "rm: '%s' 을(를) 지울 수 없습니다: %s\n", path, strerror(errno));
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) == -1) {
            if (!force)
                fprintf(stderr, "rm: '%s' 을(를) 지울 수 없습니다: %s\n", path, strerror(errno));
            return force ? 0 : 1;
        }
        return 0;
    }

    dir = opendir(path);
    if (dir == NULL) {
        if (!force)
            fprintf(stderr, "rm: '%s' 디렉토리를 열 수 없습니다: %s\n", path, strerror(errno));
        return force ? 0 : 1;
    }
    while ((e = readdir(dir)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        if (remove_tree(child, force) != 0)
            status = 1;
    }
    closedir(dir);

    if (rmdir(path) == -1) {
        if (!force)
            fprintf(stderr, "rm: '%s' 디렉토리를 지울 수 없습니다: %s\n", path, strerror(errno));
        status = force ? status : 1;
    }
    return status;
}

/* "/", ".", ".." 처럼 통째로 지우면 위험한 경로는 거부한다 */
static int is_dangerous(const char *path)
{
    char tmp[4096];
    const char *base;
    size_t n;

    snprintf(tmp, sizeof(tmp), "%s", path);
    n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/')       /* 끝의 / 는 무시한다 */
        tmp[--n] = '\0';
    if (strcmp(tmp, "/") == 0)
        return 1;
    base = strrchr(tmp, '/');
    base = base ? base + 1 : tmp;
    return strcmp(base, ".") == 0 || strcmp(base, "..") == 0;
}

int cmd_rm(int argc, char *argv[])
{
    int i, recursive = 0, force = 0, status = 0;
    const char *flag;

    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        for (flag = argv[i] + 1; *flag; flag++) {
            if (*flag == 'r' || *flag == 'R') {
                recursive = 1;
            } else if (*flag == 'f') {
                force = 1;
            } else {
                fprintf(stderr, "rm: 알 수 없는 옵션 '-%c'\n", *flag);
                return 1;
            }
        }
    }
    if (i >= argc) {
        if (!force)
            fprintf(stderr, "사용법: rm [-r] [-f] 파일...\n");
        return force ? 0 : 1;
    }

    /* 한 경로가 실패해도 나머지 경로는 계속 처리한다 */
    for (; i < argc; i++) {
        struct stat st;

        if (is_dangerous(argv[i])) {
            fprintf(stderr, "rm: '%s' 은(는) 지울 수 없는 경로입니다\n", argv[i]);
            status = 1;
            continue;
        }
        if (!recursive) {
            if (lstat(argv[i], &st) == 0 && S_ISDIR(st.st_mode)) {
                fprintf(stderr, "rm: '%s' 은(는) 디렉토리입니다 (-r 옵션이 필요합니다)\n", argv[i]);
                status = 1;
                continue;
            }
            if (unlink(argv[i]) == -1) {
                if (!(force && errno == ENOENT)) {
                    fprintf(stderr, "rm: '%s' 을(를) 지울 수 없습니다: %s\n",
                            argv[i], strerror(errno));
                    status = 1;
                }
            }
        } else if (remove_tree(argv[i], force) != 0) {
            status = 1;
        }
    }
    return status;
}
