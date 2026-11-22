/* ls [-a] [-l] [경로...]
 *  -a : 이름이 .으로 시작하는 파일도 출력
 *  -l : 권한, 링크 수, 소유자, 그룹, 크기, 수정 시간과 함께 자세히 출력
 * 터미널에 출력할 때는 한 줄에 여러 개, 파이프나 파일로 출력할 때는 한 줄에 하나씩 출력한다. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../shell.h"

static int opt_a, opt_l;

/* 대소문자를 구분하지 않고 비교하고, 같으면 원래 문자열로 비교 */
static int name_cmp(const struct dirent **a, const struct dirent **b)
{
    int r = strcasecmp((*a)->d_name, (*b)->d_name);
    return r != 0 ? r : strcmp((*a)->d_name, (*b)->d_name);
}

static void mode_string(mode_t m, char *s)
{
    s[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' :
           S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';
    s[1] = (m & S_IRUSR) ? 'r' : '-';
    s[2] = (m & S_IWUSR) ? 'w' : '-';
    s[3] = (m & S_ISUID) ? ((m & S_IXUSR) ? 's' : 'S') : ((m & S_IXUSR) ? 'x' : '-');
    s[4] = (m & S_IRGRP) ? 'r' : '-';
    s[5] = (m & S_IWGRP) ? 'w' : '-';
    s[6] = (m & S_ISGID) ? ((m & S_IXGRP) ? 's' : 'S') : ((m & S_IXGRP) ? 'x' : '-');
    s[7] = (m & S_IROTH) ? 'r' : '-';
    s[8] = (m & S_IWOTH) ? 'w' : '-';
    s[9] = (m & S_ISVTX) ? ((m & S_IXOTH) ? 't' : 'T') : ((m & S_IXOTH) ? 'x' : '-');
    s[10] = '\0';
}

/* -l 형식으로 한 줄 출력 */
static void print_long(const char *dir, const char *name)
{
    char path[4096], mode[11], when[32], link[4096];
    struct stat st;
    struct passwd *pw;
    struct group *gr;
    struct tm *tm;
    ssize_t n;

    if (dir != NULL)
        snprintf(path, sizeof(path), "%s/%s", dir, name);
    else
        snprintf(path, sizeof(path), "%s", name);
    if (lstat(path, &st) == -1) {
        fprintf(stderr, "ls: '%s': %s\n", path, strerror(errno));
        return;
    }
    mode_string(st.st_mode, mode);
    pw = getpwuid(st.st_uid);
    gr = getgrgid(st.st_gid);
    tm = localtime(&st.st_mtime);
    strftime(when, sizeof(when), "%b %e %H:%M", tm);
    printf("%s %2ld %-8s %-8s %8ld %s %s", mode, (long)st.st_nlink,
           pw ? pw->pw_name : "?", gr ? gr->gr_name : "?", (long)st.st_size, when, name);
    if (S_ISLNK(st.st_mode) && (n = readlink(path, link, sizeof(link) - 1)) > 0) {
        link[n] = '\0';
        printf(" -> %s", link);
    }
    printf("\n");
}

/* 이름 목록을 터미널 폭(80)에 맞춰 한 줄에 여러 개씩 출력 */
static void print_short(char **names, int n)
{
    int i, col = 0, w;

    if (!isatty(STDOUT_FILENO)) {
        for (i = 0; i < n; i++)
            printf("%s\n", names[i]);
        return;
    }
    for (i = 0; i < n; i++) {
        w = (int)strlen(names[i]) + 2;
        if (col + w > 80 && col > 0) {
            printf("\n");
            col = 0;
        }
        printf("%s  ", names[i]);
        col += w;
    }
    if (n > 0)
        printf("\n");
}

static int list_dir(const char *path)
{
    struct dirent **ents;
    char **names;
    int n, i, cnt = 0;

    n = scandir(path, &ents, NULL, name_cmp);
    if (n < 0) {
        fprintf(stderr, "ls: '%s'에 접근할 수 없습니다: %s\n", path, strerror(errno));
        return 1;
    }
    names = malloc(sizeof(char *) * (n + 1));
    for (i = 0; i < n; i++) {
        if (!opt_a && ents[i]->d_name[0] == '.')
            continue;
        names[cnt++] = ents[i]->d_name;
    }
    if (opt_l) {
        for (i = 0; i < cnt; i++)
            print_long(path, names[i]);
    } else {
        print_short(names, cnt);
    }
    free(names);
    for (i = 0; i < n; i++)
        free(ents[i]);
    free(ents);
    return 0;
}

int cmd_ls(int argc, char *argv[])
{
    int i, j, nfiles = 0, status = 0, ended = 0, printed = 0;
    char *files[256];
    struct stat st;

    opt_a = opt_l = 0;
    for (i = 1; i < argc; i++) {
        if (!ended && strcmp(argv[i], "--") == 0) {
            ended = 1;              /* -- 뒤에는 - 로 시작해도 경로로 본다 */
            continue;
        }
        if (!ended && argv[i][0] == '-' && argv[i][1] != '\0') {
            for (j = 1; argv[i][j]; j++) {
                if (argv[i][j] == 'a')
                    opt_a = 1;
                else if (argv[i][j] == 'l')
                    opt_l = 1;
                else {
                    fprintf(stderr, "ls: 알 수 없는 옵션 -- '%c'\n", argv[i][j]);
                    return 2;
                }
            }
        } else if (nfiles < 256) {
            files[nfiles++] = argv[i];
        }
    }
    if (nfiles == 0)
        return list_dir(".");

    /* 파일은 먼저 출력하고, 디렉토리는 그 다음에 "이름:" 머리글과 함께 출력한다 */
    for (i = 0; i < nfiles; i++) {
        if (stat(files[i], &st) == -1) {
            fprintf(stderr, "ls: '%s'에 접근할 수 없습니다: %s\n", files[i], strerror(errno));
            status = 1;
            continue;
        }
        if (S_ISDIR(st.st_mode))
            continue;
        if (opt_l)
            print_long(NULL, files[i]);
        else
            printf("%s\n", files[i]);
        printed = 1;
    }
    for (i = 0; i < nfiles; i++) {
        if (stat(files[i], &st) == -1 || !S_ISDIR(st.st_mode))
            continue;
        if (nfiles > 1) {
            if (printed)
                printf("\n");
            printf("%s:\n", files[i]);
        }
        status |= list_dir(files[i]);
        printed = 1;
    }
    return status;
}
