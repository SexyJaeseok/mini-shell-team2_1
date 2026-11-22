CC = gcc
CFLAGS = -Wall -g

SRCS = shell.c cmds/cmd_ls.c cmds/cmd_pwd.c cmds/cmd_cd.c cmds/cmd_mkdir.c

# 박재석 담당 명령어 (rmdir, rm)
SRCS += cmds/cmd_rmdir.c cmds/cmd_rm.c

myshell: $(SRCS) shell.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f myshell

.PHONY: clean
