CC = gcc
CFLAGS = -Wall -g

SRCS = shell.c cmds/cmd_ls.c cmds/cmd_cat.c cmds/cmd_grep.c cmds/cmd_pwd.c cmds/cmd_cd.c cmds/cmd_mkdir.c

# 박재석 담당 명령어 (rmdir, rm)
SRCS += cmds/cmd_rmdir.c cmds/cmd_rm.c

# 김시현 담당 명령어 (cp, mv, ln)
SRCS += cmds/cmd_cp.c cmds/cmd_mv.c cmds/cmd_ln.c

myshell: $(SRCS) shell.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f myshell

.PHONY: clean
