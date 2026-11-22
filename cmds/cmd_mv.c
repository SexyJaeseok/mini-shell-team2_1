/* 김시현 담당: mv (목적지 결정은 cmd_cp.c 의 run_transfer 를 함께 사용한다) */
#include "../shell.h"

int cmd_mv(int argc, char *argv[]) {
    return run_transfer(argv, 1);
}
