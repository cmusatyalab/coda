#include <stdio.h>
#include <signal.h>
#include <mach.h>

/*
  Zombie routine: link into applications only to find the
  RVM_LOG_TAIL_BUG
*/

void zombie__FiT1P10sigcontext(int sig, int code, struct sigcontext *scp)
{
    fprintf(stderr,"Being zombied!\n");
    fflush(stderr);
    task_suspend(task_self());
}

