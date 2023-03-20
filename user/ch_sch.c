//======================================================
//  File:           ch_sch.c
//  Author:         Lazar Vukovic
//  Module:         user
//  Date:           07/02/2022
//  Description:    User program demonstrating a system
//                  call for changing the scheduler
//======================================================

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
    int newalg = 0;
    int param = 1;

    for (int i = 1; i < argc; i++) {
        switch (i) {
            case 1:
                newalg = atoi(argv[i]);
                break;
            case 2:
                param = atoi(argv[i]);
                break;
            default:
                break;
        }
    }

//    printf("newalg = %d\n", newalg);

    if (ch_sched(newalg, param) == 0) {
        printf("Succesfully changed the scheduler!\n");
    } else {
        printf("Error changing the scheduler!\n");
    }

    exit(0);

}