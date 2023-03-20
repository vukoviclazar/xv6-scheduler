#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
// added for project
#include "scheduler.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// added for project
uint64
sys_ch_sched(void) {
    int n, param;
    enum sched_alg newalg;

    if(argint(0, &n) < 0)
        return -1;

    switch(n) {
        case 0:
            newalg = ROUND_ROBIN;

            if(argint(1, &param) < 0)
                return -1;
            if (param < 1)
                return -1;

            change_sched(newalg, param);
            break;
        case 1:
            newalg = NON_PREEMPTIVE_SJF;

            if(argint(1, &param) < 0)
                return -1;
            if (param < 0 || param > DECIMAL_ONE)
                return -1;

            change_sched(newalg, param);
            break;
        case 2:
            newalg = PREEMPTIVE_SJF;

            if(argint(1, &param) < 0)
                return -1;
            if (param < 0 || param > DECIMAL_ONE)
                return -1;

            change_sched(newalg, param);
            break;
        case 3:
            newalg = CFS;

            change_sched(newalg, 0);
            break;
        default:
            return -1;
            break;
    }
    return 0;
}
// end of added