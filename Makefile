CC = gcc
CFLAGS = -Wall -g

SRCS = shell.c cmds/cmd_ls.c cmds/cmd_cat.c cmds/cmd_grep.c cmds/cmd_pwd.c cmds/cmd_cd.c cmds/cmd_mkdir.c

myshell: $(SRCS) shell.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f myshell

.PHONY: clean
