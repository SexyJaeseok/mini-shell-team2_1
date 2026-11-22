/* rmdir [-p] 디렉토리...
 *  -p : a/b/c 를 지울 때 c, b, a 순서로 비어 있는 상위 디렉토리도 함께 지운다 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "../shell.h"

/* a/b/c 를 지우고, 이어서 a/b, a 를 차례로 지운다 (비어 있지 않으면 거기서 멈춘다) */
static int remove_with_parents(const char *path)
{
    char tmp[4096], *slash;

    snprintf(tmp, sizeof(tmp), "%s", path);
    if (rmdir(tmp) == -1)
        return -1;
    while ((slash = strrchr(tmp, '/')) != NULL && slash != tmp) {
        *slash = '\0';
        if (rmdir(tmp) == -1)
            break;       /* 상위가 비어 있지 않아도 오류로 보지 않는다 */
    }
    return 0;
}

int cmd_rmdir(int argc, char *argv[])
{
    int i, parents = 0, status = 0;

    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            parents = 1;
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else {
            fprintf(stderr, "rmdir: 알 수 없는 옵션 '%s'\n", argv[i]);
            return 1;
        }
    }
    if (i >= argc) {
        fprintf(stderr, "사용법: rmdir [-p] 디렉토리...\n");
        return 1;
    }

    /* 한 경로가 실패해도 나머지 경로는 계속 처리한다 */
    for (; i < argc; i++) {
        int r = parents ? remove_with_parents(argv[i]) : rmdir(argv[i]);
        if (r == -1) {
            fprintf(stderr, "rmdir: '%s' 디렉토리를 지울 수 없습니다: %s\n",
                    argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}
