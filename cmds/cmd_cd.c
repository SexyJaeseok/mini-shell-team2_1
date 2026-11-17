/* cd : 작업 디렉토리를 바꾼다.
 *  cd          -> 홈 디렉토리
 *  cd -        -> 바로 이전 디렉토리
 *  cd ~/dir    -> 홈 디렉토리 기준 경로
 * 작업 디렉토리는 쉘 프로세스의 속성이므로 쉘이 직접(자식이 아니라) 실행한다. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../shell.h"

int cmd_cd(int argc, char *argv[])
{
    char old[PATH_MAX], cur[PATH_MAX], path[PATH_MAX];
    const char *target, *home = getenv("HOME");
    int print_dir = 0;

    if (argc > 2) {
        fprintf(stderr, "cd: 인자가 너무 많습니다\n");
        return 1;
    }
    if (argc == 1) {
        target = home;
    } else if (strcmp(argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        print_dir = 1;
    } else if (argv[1][0] == '~' && (argv[1][1] == '\0' || argv[1][1] == '/') && home != NULL) {
        snprintf(path, sizeof(path), "%s%s", home, argv[1] + 1);
        target = path;
    } else {
        target = argv[1];
    }
    if (target == NULL) {
        fprintf(stderr, "cd: HOME 또는 OLDPWD 가 설정되어 있지 않습니다\n");
        return 1;
    }

    if (getcwd(old, sizeof(old)) == NULL)
        old[0] = '\0';
    if (chdir(target) == -1) {
        fprintf(stderr, "cd: %s: ", target);
        perror("");
        return 1;
    }
    if (getcwd(cur, sizeof(cur)) != NULL) {
        setenv("OLDPWD", old, 1);
        setenv("PWD", cur, 1);
        if (print_dir)
            printf("%s\n", cur);
    }
    return 0;
}
