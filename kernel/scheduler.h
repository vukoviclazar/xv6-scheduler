//======================================================
//  File:           scheduler.h
//  Author:         Lazar Vukovic
//  Module:         kernel
//  Date:           05/02/2022
//  Description:    Declarations of data and functions
//                  required for process scheduling.
//======================================================

// constant for calculating execution time prediction
// used since doubles aren't supported
#define DECIMAL_ONE 100000

struct proc* get();
void put(struct proc *p);

enum sched_alg { ROUND_ROBIN, NON_PREEMPTIVE_SJF, PREEMPTIVE_SJF, CFS };

int should_yield();
void change_sched(enum sched_alg newalg, int param);

extern struct spinlock sched_lock;
