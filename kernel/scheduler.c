//======================================================
//  File:           scheduler.c
//  Author:         Lazar Vukovic
//  Module:         kernel
//  Date:           05/02/2022
//  Description:    Definitions of data and functions
//                  required for process scheduling.
//======================================================

// mostly copied from proc.c
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "scheduler.h"

// largest value fitting into int
#define P_INF_INT (int)(~0U >> 1);

// lock protecting scheduler data
struct spinlock sched_lock;
// currently active scheduler
enum sched_alg active_scheduler = ROUND_ROBIN;
// other scheduler data
int rr_cursor = 0;
int rr_quantum = 1;
int alpha = 50000;
int proc_cnt = 0;

// function used from the system call ch_sch,
// changes the scheduling algorithm
void change_sched(enum sched_alg newalg, int param) {
    char* str;

    acquire(&sched_lock);
    active_scheduler = newalg;

    switch(newalg) {
        case NON_PREEMPTIVE_SJF:
            alpha = param;
            str = "NON_PREEMPTIVE_SJF";
            break;
        case PREEMPTIVE_SJF:
            alpha = param;
            str = "PREEMPTIVE_SJF";
            break;
        case CFS:
            str = "CFS";
            break;
        case ROUND_ROBIN:
            rr_quantum = param;
            str = "ROUND_ROBIN";
            break;
    }
    printf("SWITCH: scheduler: %s, alpha = %d, rr_quantum = %d\n", str, alpha, rr_quantum);
    release(&sched_lock);
}

// function used in kerneltrap and usertrap to determine
// whether a change of context is necessary

int should_yield(){
    enum sched_alg curr_alg;
    int retval = 1;
    int quant;

    struct proc *p = myproc();

    acquire(&sched_lock);
    curr_alg = active_scheduler;
    quant = rr_quantum;
    release(&sched_lock);

    acquire(&p->lock);

    switch(curr_alg) {
        case NON_PREEMPTIVE_SJF:
            retval = 0;
            break;
        case PREEMPTIVE_SJF:
            retval = 1;
            break;
        case CFS:
            retval = (p->running_time % p->quantum == 0);
            break;
        case ROUND_ROBIN:
            retval = (p->running_time % quant == 0);
            break;
    }

    release(&p->lock);
    return retval;
}
// a helper function for the cfs algorithm

void update_proc_cfs(struct proc *p) {
    uint t;
    int q;
    int cnt;

//    acquire(&tickslock);
    t = ticks;
//    release(&tickslock);

    acquire(&sched_lock);
    cnt = proc_cnt--;
    release(&sched_lock);

    q = (int)(t - p->time_entered) / cnt;
    p->quantum = q ? q : 1;
}
// get the next process for execution using the original round-robin algorithm

struct proc* get_rr() {
    int i, mycursor;
    // only one kernel thread can modify scheduler data at once

    acquire(&sched_lock);
    mycursor=rr_cursor;
    release(&sched_lock);

    // round robin algorithm
    for(i = 0; i < NPROC; i++, mycursor = (mycursor + 1) % NPROC) {
        // acquire lock to inspect process data
        acquire(&proc[mycursor].lock);
        if (proc[mycursor].state == RUNNABLE) {

            acquire(&sched_lock);
            rr_cursor=mycursor+1;
            proc_cnt--;
            release(&sched_lock);

//            push_off();
//            printf("SCHEDULER GET: minimal_time = %d, process %d, cpu %d\n", proc[mycursor].tau - proc[mycursor].running_time, proc[mycursor].pid, cpuid());
//            pop_off();
            // keep process data locked, because it is not done modifying
            return &proc[mycursor];
        }
        release(&proc[mycursor].lock);
    }
    // in case no processes are ready, return null
    return 0;
}

// get the next process for execution using the shortest job first algorithm
struct proc* get_sjf() {
    int min_t = P_INF_INT;
    int i, ind, found;

    while (1) {
        found = 0;
        for (i = 0, ind = 0; i < NPROC; i++) {

            acquire(&proc[i].lock);
            if (proc[i].state == RUNNABLE) {
//                // ONE POSSIBLE REASONING FOR THE SJF ALGORITHM
//                // CHOOSE THE PROCESS WITH TAU ZERO ONLY IF THERE ARE NO OTHERS
//
//                // in case there are no processes with tau different from 0
//                // a process with p->tau == 0 will be returned
//                if (proc[i].tau == 0) {
//                    if (!found) {
//                        found = 1;
//                        ind = i;
//                        min_t = P_INF_INT;
//                    }
//                }
//                // if tau is different from 0 (finite execution prediction)
//                // select the process with minimal possible time left
//                else if (proc[i].tau - proc[i].running_time < min_t) {
//                    found = 1;
//                    ind = i;
//                    min_t = proc[i].tau - proc[i].running_time;
//                }
                // OTHER POSSIBLE REASONING FOR THE SJF ALGORITHM
                // GIVE A CHANCE TO EVERY PROCESS WITH TAU ZERO FIRST

                // no need to look further, return the current process
                if (proc[i].tau == 0) {

                    acquire(&sched_lock);
                    proc_cnt--;
                    release(&sched_lock);

//                    push_off();
//                    printf("SCHEDULER GET: minimal_time = %d, process %d, cpu %d\n", proc[i].tau - proc[i].running_time, proc[i].pid, cpuid());
//                    pop_off();
                    return &proc[i];
                }
                // find the least predicted remaining execution time,
                // even if the process has executed longer than predicted
                // (in that case it may be closer to finishing)
                if (proc[i].tau - proc[i].running_time < min_t) {
                    found = 1;
                    ind = i;
                    min_t = proc[i].tau - proc[i].running_time;
                }
            }
            release(&proc[i].lock);
        }
        // if there are no processes ready, return null
        if (found == 0) {
            return 0;
        }
        // check again if the process is ready,
        // as it may have changed in the meantime
        acquire(&proc[ind].lock);
        if (proc[ind].state == RUNNABLE) {
//            push_off();
//            printf("SCHEDULER GET: minimal_time = %d, process %d, cpu %d\n", proc[ind].tau - proc[ind].running_time, proc[ind].pid, cpuid());
//            pop_off();
            acquire(&sched_lock);
            proc_cnt--;
            release(&sched_lock);

            return &proc[ind];
        }
        release(&proc[ind].lock);
        // in case the chosen process is not ready anymore
        // search again
    }
}

// get the next process for execution using the completely fair scheduler algorithm
struct proc* get_cfs() {
    int min_t = P_INF_INT;
    int i, ind, found;
    while (1) {
        found = 0;

        acquire(&sched_lock);
        if (proc_cnt == 0) {
            release(&sched_lock);

            return 0;
        }
        release(&sched_lock);

        for (i = 0, ind = 0; i < NPROC; i++) {

            acquire(&proc[i].lock);
            if (proc[i].state == RUNNABLE) {

                // no need to look further, 0 is the least possible running_time
                if (proc[i].running_time == 0) {
                    update_proc_cfs(&proc[i]);
//                    push_off();
//                    printf("SCHEDULER GET: quantum = %d, running_time = %d, process %d, cpu %d\n", proc[i].quantum, proc[i].running_time, proc[i].pid, cpuid());
//                    pop_off();
                    return &proc[i];
                }

                // find the process that executed for the least amount of time
                if (proc[i].running_time < min_t) {
                    found = 1;
                    ind = i;
                    min_t = proc[i].running_time;
                }
            }
            release(&proc[i].lock);
        }
        // if there are no processes ready, return null
        if (found == 0) {
            return 0;
        }
        // check again if the process is ready,
        // as it may have changed in the meantime
        acquire(&proc[ind].lock);
        if (proc[ind].state == RUNNABLE) {
            update_proc_cfs(&proc[ind]);

//            push_off();
//            printf("SCHEDULER GET: quantum = %d, running_time = %d, process %d, cpu %d\n", proc[ind].quantum, proc[ind].running_time, proc[ind].pid, cpuid());
//            pop_off();
            return &proc[ind];
        }
        release(&proc[ind].lock);
    }
}

// scheduler function that returns the next process to be run, updating scheduler data if needed
// the returned process has its state locked because the context switch is not over
struct proc *get() {
    enum sched_alg curr_alg;

    acquire(&sched_lock);
    curr_alg = active_scheduler;
    release(&sched_lock);

    switch (curr_alg) {
        case NON_PREEMPTIVE_SJF:
        case PREEMPTIVE_SJF:
            return get_sjf();
            break;
        case CFS:
            return get_cfs();
            break;
        case ROUND_ROBIN:
            return get_rr();
            break;
    }
    return 0;
}

// put the process p in the scheduler using round-robin algorithm - does nothing
void put_rr(struct proc *p) {}

// put the process p in the scheduler using the sjf algorithm and update respective process data
void put_sjf(struct proc *p) {
    int t;
    // update process data only if this process' cpu burst is over (p->from_suspension == 1)
    if (p->from_suspension) {
        p->from_suspension = 0;
        // p->tau equals 0 only at the end of the first cpu burst,
        // in that case set the prediction to the length of the burst
        if (p->tau == 0)
            p->tau = p->running_time ? p->running_time : 1;
        else {
            // at the end of the n-th cpu burst, predict the length of the next
            t = (alpha * (p->running_time) + (DECIMAL_ONE - alpha) * p->tau) / DECIMAL_ONE;
            // since 0 is a special value, the least value assigned to tau can be 1
            p->tau = t ? t : 1;
        }
        // reset the length of cpu burst
        p->running_time = 0;
//        printf("SCHEDULER PUT UPDATE: tau = %d, process %d\n", p->tau, p->pid);
    }
//    printf("SCHEDULER PUT: running_time = %d, process %d\n", p->running_time, p->pid);
}

// put the process p in the scheduler using the cfs algorithm and update respective process data
void put_cfs(struct proc *p) {
//    if (!holding(&tickslock)) {
//        acquire(&tickslock);
//        p->time_entered = ticks;
//        release(&tickslock);
//    } else {
        p->time_entered = ticks;
//    }
    acquire(&sched_lock);
    proc_cnt++;
    release(&sched_lock);
    if (p->from_suspension) {
        p->from_suspension = 0;
        // reset the length of cpu burst
        p->running_time = 0;
    }
//    printf("SCHEDULER PUT: old quantum = %d, running_time = %d, process %d\n", p->quantum, p->running_time, p->pid);
}

// scheduler function that marks process as ready to be run, updating scheduler data
void put(struct proc *p) {
    int t, myalpha;
//    access to ticks variable should be protected by tickslock
//    however that would deadlock if more than one processor is
//    active, I think if the variable is only read outside clockintr()
//    that it satisfies at-most-once-property

//    if (!holding(&tickslock)) {
//        acquire(&tickslock);
//        p->time_entered = ticks;
//        release(&tickslock);
//    } else {
        p->time_entered = ticks;
//    }

    acquire(&sched_lock);
    proc_cnt++;
    myalpha = alpha;
    release(&sched_lock);

    // update other process data only if the cpu burst is over
    if (p->from_suspension) {
        p->from_suspension = 0;
        // p->tau equals 0 only at the end of the first cpu burst,
        // in that case set the prediction to the length of the burst
        if (p->tau == 0)
            p->tau = p->running_time ? p->running_time : 1;
        else {
            // at the end of the n-th cpu burst, predict the length of the next
            t = (myalpha * (p->running_time) + (DECIMAL_ONE - myalpha) * p->tau) / DECIMAL_ONE;
            // since 0 is a special value, the least value assigned to tau can be 1
            p->tau = t ? t : 1;
        }
        // reset the length of cpu burst
        p->running_time = 0;
    }
//    push_off();
//    printf("SCHEDULER PUT: old quantum = %d, running_time = %d, tau = %d, process %d, cpu %d\n", p->quantum, p->running_time, p->tau, p->pid, cpuid());
//    pop_off();
}
