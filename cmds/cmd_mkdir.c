/* mkdir [-p] [-m 모드] 디렉토리...
 *  -p : 중간 경로도 함께 만들고, 이미 있어도 오류로 보지 않는다 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../shell.h"

/* a/b/c 를 만들 때 a, a/b, a/b/c 를 차례로 만든다 */
static int make_parents(const char *path, mode_t mode)
{
    char tmp[4096], *p;
    struct stat st;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for (p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(tmp, mode) == -1 && errno != EEXIST)
            return -1;
        *p = '/';
    }
    if (mkdir(tmp, mode) == -1) {
        if (errno == EEXIST && stat(tmp, &st) == 0 && S_ISDIR(st.st_mode))
            return 0;
        return -1;
    }
    return 0;
}

int cmd_mkdir(int argc, char *argv[])
{
    int i, parents = 0, status = 0, first = 1;
    mode_t mode = 0777;      /* umask 가 적용되어 보통 755 가 된다 */

    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++, first = i) {
        if (strcmp(argv[i], "-p") == 0) {
            parents = 1;
        } else if (strcmp(argv[i], "-m") == 0) {
            char *end;
            if (i + 1 >= argc) {
                fprintf(stderr, "mkdir: -m 옵션에는 모드가 필요합니다\n");
                return 1;
            }
            mode = (mode_t)strtol(argv[++i], &end, 8);
            if (*argv[i] == '\0' || *end != '\0' || mode > 07777) {
                fprintf(stderr, "mkdir: 잘못된 모드입니다: '%s'\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else {
            fprintf(stderr, "mkdir: 알 수 없는 옵션 '%s'\n", argv[i]);
            return 1;
        }
    }
    first = i;
    if (first >= argc) {
        fprintf(stderr, "사용법: mkdir [-p] [-m 모드] 디렉토리...\n");
        return 1;
    }

    for (i = first; i < argc; i++) {
        int r = parents ? make_parents(argv[i], mode) : mkdir(argv[i], mode);
        if (r == -1) {
            fprintf(stderr, "mkdir: '%s' 디렉토리를 만들 수 없습니다: %s\n",
                    argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}
