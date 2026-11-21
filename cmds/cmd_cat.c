/* 임채민 담당: cat의 파일 및 표준 입력 출력 */
#define _POSIX_C_SOURCE 200809L

#include "../shell.h"
#include <stdio.h>
#include <string.h>

static int copy_stream(FILE *src, FILE *dst) {
    char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, n, dst) != n)
            return 1;
    }

    return ferror(src) || ferror(dst);
}

int cmd_cat(int argc, char *argv[]) {
    /* 파일명이 없으면 표준 입력을 출력한다. */
    if (argc == 1)
        return copy_stream(stdin, stdout);

    int result = 0;

    for (int i = 1; i < argc; i++) {
        int use_stdin = !strcmp(argv[i], "-");
        FILE *file = use_stdin ? stdin : fopen(argv[i], "rb");

        if (!file) {
            perror(argv[i]);
            result = 1;
            continue;
        }

        if (copy_stream(file, stdout))
            result = 1;

        if (!use_stdin)
            fclose(file);
    }

    return result;
}
