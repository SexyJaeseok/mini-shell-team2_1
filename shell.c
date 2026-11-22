/* 간단한 쉘 프로그램
 *  - exit 로 종료
 *  - 명령 끝에 & 를 붙이면 백그라운드로 실행
 *  - Ctrl-C(SIGINT), Ctrl-\(SIGQUIT), Ctrl-Z(SIGTSTP) 는 실행 중인 명령(포그라운드)에만 전달
 *  - 파일 재지향(<, >, >>)과 파이프(|)
 *  - ls, pwd, cd, mkdir 은 cmds/ 에 직접 구현, 나머지는 시스템 명령어 실행
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "shell.h"

/* ---------- 내부 명령어 표 ---------- */
static int do_jobs(int argc, char *argv[]);
static int do_fg(int argc, char *argv[]);
static int do_bg(int argc, char *argv[]);

struct builtin {
    const char *name;
    cmd_fn fn;
    int in_parent;      /* 1 이면 쉘 프로세스에서 직접 실행 (cd, jobs, fg, bg) */
};

static struct builtin builtins[] = {
    { "ls",    cmd_ls,    0 },
    { "cat",   cmd_cat,   0 },
    { "grep",  cmd_grep,  0 },
    { "pwd",   cmd_pwd,   0 },
    { "mkdir", cmd_mkdir, 0 },
    { "rmdir", cmd_rmdir, 0 },
    { "rm",    cmd_rm,    0 },
    { "cd",    cmd_cd,    1 },
    { "jobs",  do_jobs,   1 },
    { "fg",    do_fg,     1 },
    { "bg",    do_bg,     1 },
    /* 팀원 담당: rmdir, ln, cp, rm, mv, cat, grep (구현되면 여기에 추가) */
    { NULL, NULL, 0 }
};

static struct builtin *find_builtin(const char *name)
{
    int i;
    for (i = 0; builtins[i].name != NULL; i++)
        if (strcmp(builtins[i].name, name) == 0)
            return &builtins[i];
    return NULL;
}

/* ---------- 작업(job) 관리 ---------- */
struct job {
    int used;
    int id;
    pid_t pgid;
    pid_t pids[MAXCMDS];
    int npids;
    int alive;          /* 아직 끝나지 않은 프로세스 수 */
    int stopped;
    char cmd[256];
};

static struct job jobs[MAXJOBS];
static pid_t shell_pgid;
static int interactive;

static struct job *add_job(pid_t pgid, pid_t *pids, int n, const char *cmd)
{
    int i, j, id;
    for (i = 0; i < MAXJOBS; i++)
        if (!jobs[i].used)
            break;
    if (i == MAXJOBS)
        return NULL;
    /* 사용 중이지 않은 가장 작은 번호 */
    for (id = 1; ; id++) {
        for (j = 0; j < MAXJOBS; j++)
            if (jobs[j].used && jobs[j].id == id)
                break;
        if (j == MAXJOBS)
            break;
    }
    jobs[i].used = 1;
    jobs[i].id = id;
    jobs[i].pgid = pgid;
    jobs[i].npids = n;
    memcpy(jobs[i].pids, pids, n * sizeof(pid_t));
    jobs[i].alive = n;
    jobs[i].stopped = 0;
    snprintf(jobs[i].cmd, sizeof(jobs[i].cmd), "%s", cmd);
    return &jobs[i];
}

static struct job *find_job_by_pid(pid_t pid)
{
    int i, k;
    for (i = 0; i < MAXJOBS; i++) {
        if (!jobs[i].used)
            continue;
        for (k = 0; k < jobs[i].npids; k++)
            if (jobs[i].pids[k] == pid)
                return &jobs[i];
    }
    return NULL;
}

static struct job *find_job_by_id(int id)
{
    int i;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].used && jobs[i].id == id)
            return &jobs[i];
    return NULL;
}

static struct job *last_job(void)
{
    int i, best = -1;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].used && (best < 0 || jobs[i].id > jobs[best].id))
            best = i;
    return best < 0 ? NULL : &jobs[best];
}

/* 포그라운드 작업이 끝나거나 멈출 때까지 기다린다 */
static void wait_fg(struct job *j)
{
    int st, interrupted = 0;
    pid_t pid;

    j->stopped = 0;
    if (!interactive) {
        /* 터미널이 아닐 때는 프로세스 그룹을 만들지 않으므로 pid 로 직접 기다린다 */
        int k;
        for (k = 0; k < j->npids; k++)
            waitpid(j->pids[k], &st, 0);
        j->used = 0;
        return;
    }
    while (j->alive > 0) {
        pid = waitpid(-j->pgid, &st, WUNTRACED);
        if (pid < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (WIFSTOPPED(st)) {
            j->stopped = 1;
            printf("\n[%d]+  Stopped\t\t%s\n", j->id, j->cmd);
            return;
        }
        j->alive--;
        if (WIFSIGNALED(st) && WTERMSIG(st) == SIGINT)
            interrupted = 1;
    }
    if (interrupted)
        printf("\n");
    j->used = 0;
}

/* 끝난 백그라운드 작업을 회수하고 알려 준다 */
static void reap_bg(void)
{
    int st;
    pid_t pid;
    struct job *j;

    while ((pid = waitpid(-1, &st, WNOHANG | WUNTRACED)) > 0) {
        j = find_job_by_pid(pid);
        if (j == NULL)
            continue;
        if (WIFSTOPPED(st)) {
            if (!j->stopped) {
                j->stopped = 1;
                printf("[%d]+  Stopped\t\t%s\n", j->id, j->cmd);
            }
            continue;
        }
        if (--j->alive == 0) {
            printf("[%d]+  Done\t\t%s\n", j->id, j->cmd);
            j->used = 0;
        }
    }
}

static void give_terminal(pid_t pgid)
{
    if (interactive)
        tcsetpgrp(STDIN_FILENO, pgid);
}

static int do_jobs(int argc, char *argv[])
{
    int i;
    for (i = 0; i < MAXJOBS; i++)
        if (jobs[i].used)
            printf("[%d]  %-8s\t%s\n", jobs[i].id,
                   jobs[i].stopped ? "Stopped" : "Running", jobs[i].cmd);
    return 0;
}

static struct job *job_from_arg(int argc, char *argv[])
{
    int id;
    if (argc < 2)
        return last_job();
    id = atoi(argv[1][0] == '%' ? argv[1] + 1 : argv[1]);
    return find_job_by_id(id);
}

static int do_fg(int argc, char *argv[])
{
    struct job *j = job_from_arg(argc, argv);
    if (j == NULL) {
        fprintf(stderr, "fg: 해당 작업이 없습니다\n");
        return 1;
    }
    printf("%s\n", j->cmd);
    give_terminal(j->pgid);
    if (j->stopped) {
        j->stopped = 0;
        kill(-j->pgid, SIGCONT);
    }
    wait_fg(j);
    give_terminal(shell_pgid);
    return 0;
}

static int do_bg(int argc, char *argv[])
{
    struct job *j = job_from_arg(argc, argv);
    if (j == NULL) {
        fprintf(stderr, "bg: 해당 작업이 없습니다\n");
        return 1;
    }
    if (j->stopped) {
        j->stopped = 0;
        kill(-j->pgid, SIGCONT);
    }
    printf("[%d]+ %s &\n", j->id, j->cmd);
    return 0;
}

/* ---------- 입력 해석 ---------- */
/* line 을 토큰으로 나누어 buf 에 복사한다. 따옴표("", '')는 묶어서 한 토큰으로 보고,
 * 따옴표 밖의 | < > >> & 는 연산자 토큰으로 분리한다. (tok 은 buf 안을 가리킨다)
 * 닫히지 않은 따옴표가 있으면 -2, 토큰이 너무 많으면 -1 을 돌려준다. */
static int tokenize(const char *line, char *buf, char **tok, int *isop, int max)
{
    const char *p = line;
    char *out = buf;
    int n = 0;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        if (n >= max - 1)
            return -1;
        if (strchr("|<>&", *p)) {
            tok[n] = out;
            isop[n] = 1;
            *out++ = *p;
            if (*p == '>' && p[1] == '>') {
                *out++ = '>';
                p++;
            }
            p++;
            *out++ = '\0';
            n++;
            continue;
        }
        tok[n] = out;
        isop[n] = 0;
        while (*p && *p != ' ' && *p != '\t' && !strchr("|<>&", *p)) {
            if (*p == '"' || *p == '\'') {
                char q = *p++;
                while (*p && *p != q)
                    *out++ = *p++;
                if (*p != q)
                    return -2;      /* 닫는 따옴표가 없다 */
                p++;
            } else {
                *out++ = *p++;
            }
        }
        *out++ = '\0';
        n++;
    }
    tok[n] = NULL;
    return n;
}

static int parse(const char *line, char *buf, Pipeline *pl)
{
    char *tok[MAXARGS * 2];
    int isop[MAXARGS * 2];
    int n, i;
    Cmd *c;

    memset(pl, 0, sizeof(*pl));
    n = tokenize(line, buf, tok, isop, MAXARGS * 2);
    if (n == -2) {
        fprintf(stderr, "myshell: 따옴표가 닫히지 않았습니다\n");
        return -1;
    }
    if (n < 0) {
        fprintf(stderr, "myshell: 입력이 너무 깁니다\n");
        return -1;
    }
    if (n == 0)
        return 0;

    pl->ncmds = 1;
    c = &pl->cmds[0];
    for (i = 0; i < n; i++) {
        if (isop[i] && strcmp(tok[i], "|") == 0) {
            if (c->argc == 0 || i + 1 >= n) {
                fprintf(stderr, "myshell: | 앞뒤에 명령이 필요합니다\n");
                return -1;
            }
            if (pl->ncmds >= MAXCMDS) {
                fprintf(stderr, "myshell: 파이프가 너무 많습니다\n");
                return -1;
            }
            c = &pl->cmds[pl->ncmds++];
        } else if (isop[i] && strcmp(tok[i], "&") == 0) {
            if (i != n - 1) {
                fprintf(stderr, "myshell: & 는 명령의 맨 끝에만 쓸 수 있습니다\n");
                return -1;
            }
            pl->background = 1;
        } else if (isop[i]) {       /* <, >, >> */
            if (i + 1 >= n || isop[i + 1]) {
                fprintf(stderr, "myshell: %s 뒤에 파일 이름이 필요합니다\n", tok[i]);
                return -1;
            }
            if (tok[i][0] == '<') {
                c->infile = tok[++i];
            } else {
                c->outfile = tok[++i];
                c->append = (strcmp(tok[i - 1], ">>") == 0);
            }
        } else {
            if (c->argc >= MAXARGS - 1) {
                fprintf(stderr, "myshell: 인자가 너무 많습니다\n");
                return -1;
            }
            c->argv[c->argc++] = tok[i];
            c->argv[c->argc] = NULL;
        }
    }
    if (c->argc == 0) {
        fprintf(stderr, "myshell: 명령이 비어 있습니다\n");
        return -1;
    }
    return pl->ncmds;
}

/* ---------- 실행 ---------- */
static void child_setup_redirect(Cmd *c)
{
    int fd;
    if (c->infile) {
        fd = open(c->infile, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", c->infile, strerror(errno));
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->outfile) {
        fd = open(c->outfile, O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC), 0644);
        if (fd < 0) {
            fprintf(stderr, "myshell: %s: %s\n", c->outfile, strerror(errno));
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

static void run_pipeline(Pipeline *pl, const char *text)
{
    int i, prev = -1, fd[2];
    pid_t pid, pgid = 0, pids[MAXCMDS];
    struct builtin *b;
    struct job *j;

    /* cd, jobs, fg, bg 는 쉘 자신이 실행해야 한다 */
    if (pl->ncmds == 1) {
        b = find_builtin(pl->cmds[0].argv[0]);
        if (b != NULL && b->in_parent) {
            b->fn(pl->cmds[0].argc, pl->cmds[0].argv);
            return;
        }
    }

    for (i = 0; i < pl->ncmds; i++) {
        Cmd *c = &pl->cmds[i];
        if (i < pl->ncmds - 1 && pipe(fd) < 0) {
            perror("pipe");
            return;
        }
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }
        if (pid == 0) {
            if (interactive) {
                setpgid(0, pgid);       /* pgid 가 0 이면 자기 pid 가 그룹 id */
                if (!pl->background)
                    tcsetpgrp(STDIN_FILENO, getpgrp());
            }
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTIN, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if (prev != -1) {
                dup2(prev, STDIN_FILENO);
                close(prev);
            } else if (pl->background && c->infile == NULL) {
                int nul = open("/dev/null", O_RDONLY);
                dup2(nul, STDIN_FILENO);
                close(nul);
            }
            if (i < pl->ncmds - 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }
            child_setup_redirect(c);

            b = find_builtin(c->argv[0]);
            if (b != NULL)
                exit(b->fn(c->argc, c->argv));
            execvp(c->argv[0], c->argv);
            fprintf(stderr, "myshell: %s: 명령을 찾을 수 없습니다\n", c->argv[0]);
            exit(127);
        }
        if (pgid == 0)
            pgid = pid;
        if (interactive)
            setpgid(pid, pgid);
        pids[i] = pid;
        if (prev != -1)
            close(prev);
        if (i < pl->ncmds - 1) {
            close(fd[1]);
            prev = fd[0];
        }
    }

    j = add_job(pgid, pids, pl->ncmds, text);
    if (j == NULL) {
        fprintf(stderr, "myshell: 작업이 너무 많습니다\n");
        return;
    }
    if (pl->background) {
        printf("[%d] %d\n", j->id, (int)pgid);
    } else {
        give_terminal(pgid);
        wait_fg(j);
        give_terminal(shell_pgid);
    }
}

int main(void)
{
    char line[LINESIZE], copy[LINESIZE];
    static char tokbuf[LINESIZE * 2 + 8];     /* 토큰이 저장되는 곳 (실행이 끝날 때까지 유지) */
    Pipeline pl;
    size_t len;

    /* 자식의 cat과 grep이 다음 셸 명령을 입력으로 물려받지 않게 한다. */
    setvbuf(stdin, NULL, _IONBF, 0);
    interactive = isatty(STDIN_FILENO);
    if (interactive) {
        /* 쉘 자신은 Ctrl-C, Ctrl-\, Ctrl-Z 로 죽거나 멈추지 않게 한다 */
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        shell_pgid = getpid();
        setpgid(shell_pgid, shell_pgid);
        tcsetpgrp(STDIN_FILENO, shell_pgid);
    }

    while (1) {
        reap_bg();
        if (interactive) {
            printf("myshell$ ");
            fflush(stdout);
        }
        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (interactive)
                printf("\n");
            break;
        }
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        } else if (len == sizeof(line) - 1) {
            /* 한 줄이 버퍼보다 길면 나머지를 버린다. 남겨 두면 다음 명령으로 읽힌다. */
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            fprintf(stderr, "myshell: 입력이 너무 깁니다\n");
            continue;
        }
        snprintf(copy, sizeof(copy), "%s", line);

        if (parse(line, tokbuf, &pl) <= 0)
            continue;
        if (strcmp(pl.cmds[0].argv[0], "exit") == 0)
            break;
        run_pipeline(&pl, copy);
    }
    return 0;
}
