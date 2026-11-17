/* pwd : 현재 작업 디렉토리의 절대 경로를 출력한다 */
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include "../shell.h"

int cmd_pwd(int argc, char *argv[])
{
    char buf[PATH_MAX];

    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", buf);
    return 0;
}
