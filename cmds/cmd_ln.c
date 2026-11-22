/* 김시현 담당: ln [-s] source link */
#define _POSIX_C_SOURCE 200809L

#include "../shell.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int cmd_ln(int argc, char *argv[]) {
    int symbolic = argv[1] &&
                   strcmp(argv[1], "-s") == 0;

    char *src = symbolic ? argv[2] : argv[1];
    char *dst = symbolic ? argv[3] : argv[2];

    if (!src || !dst) {
        fprintf(stderr, "Usage: ln [-s] source link\n");
        return 1;
    }

    int result = symbolic ? symlink(src, dst)
                          : link(src, dst);

    if (result == -1) {
        perror("ln");
        return 1;
    }

    return 0;
}
