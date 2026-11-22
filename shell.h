#ifndef SHELL_H
#define SHELL_H

#define MAXARGS 64
#define MAXCMDS 16
#define MAXJOBS 32
#define LINESIZE 1024

/* 파이프라인의 명령 하나 */
typedef struct {
    char *argv[MAXARGS];
    int argc;
    char *infile;      /* < */
    char *outfile;     /* > 또는 >> */
    int append;        /* >> 이면 1 */
} Cmd;

/* 한 줄 입력을 해석한 결과 (cmd1 | cmd2 | ... [&]) */
typedef struct {
    Cmd cmds[MAXCMDS];
    int ncmds;
    int background;
} Pipeline;

/* 명령어 구현 함수: main 과 같은 모양, 반환값은 종료 코드 */
typedef int (*cmd_fn)(int argc, char *argv[]);

/* ---- 명령어 구현 (cmds/ 디렉토리) ----
 * 팀원이 맡은 명령어도 같은 모양으로 cmds/cmd_xxx.c 에 만들고
 * shell.c 의 builtins[] 표에 이름과 함수를 추가하면 된다.
 * 표에 없는 명령어는 execvp() 로 시스템 명령어를 실행한다. */
int cmd_ls(int argc, char *argv[]);
int cmd_pwd(int argc, char *argv[]);
int cmd_cd(int argc, char *argv[]);
int cmd_rmdir(int argc, char *argv[]);
int cmd_rm(int argc, char *argv[]);
int cmd_mkdir(int argc, char *argv[]);

#endif
