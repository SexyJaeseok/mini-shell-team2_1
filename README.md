# team_shell

오픈소스프로젝트 팀프로젝트 1. 쉘프로그램 제작 (팀 3명)

## 기능
| 번호 | 요구사항 | 구현 |
|---|---|---|
| 1 | `exit` 입력 시 종료 | `shell.c` main 루프 (Ctrl-D 도 종료) |
| 2 | 명령 끝에 `&` 를 붙이면 백그라운드 실행 | 프로세스 그룹 + 작업(job) 표, `jobs` `fg` `bg` |
| 3 | 인터럽트 키 | 쉘은 SIGINT/SIGQUIT/SIGTSTP 를 무시하고 포그라운드 작업에만 전달 (Ctrl-C 종료, Ctrl-Z 정지) |
| 4 | `>` `>>` `<` `\|` | `tokenize()` / `parse()` / `run_pipeline()` |
| 5 | 명령어 분담 구현 | 아래 표 |

## 명령어 분담 (11개를 3 / 4 / 4 개로)
| 담당 | 명령어 | 위치 |
|---|---|---|
| 임채민 | `cat`, `grep`, `pwd` | `cmds/cmd_cat.c`, `cmds/cmd_grep.c`, `cmds/cmd_pwd.c` |
| 박재석 | `ls`, `mkdir`, `rmdir`, `rm` | `cmds/cmd_ls.c` `cmd_mkdir.c` (작성됨), `cmd_rmdir.c` `cmd_rm.c` (아직 시스템 명령어 사용) |
| 김시현 | `cd`, `cp`, `mv`, `ln` | `cmds/cmd_cd.c`, `cmds/cmd_cp.c`, `cmds/cmd_mv.c`, `cmds/cmd_ln.c` |

## 팀원이 명령어를 추가하는 방법
1. `cmds/cmd_xxx.c` 에 `int cmd_xxx(int argc, char *argv[])` 를 만든다. (반환값은 종료 코드)
2. `shell.h` 에 선언을 추가한다.
3. `shell.c` 의 `builtins[]` 표에 `{ "xxx", cmd_xxx, 0 }` 를 추가한다.
4. `Makefile` 의 `SRCS` 에 파일을 추가한다.

표에 없는 명령어는 `execvp()` 로 시스템 명령어를 실행하므로, 구현되기 전에도 쉘은 동작한다.
`cd` 처럼 쉘 자신의 상태를 바꾸는 명령어만 세 번째 값을 1 로 한다.

## 빌드와 실행
```
make
./myshell
```
