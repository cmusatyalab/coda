#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./lib-src/mlwp/tdb.c,v 1.1 1996/11/22 19:18:52 braam Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "lwp.h"

int procA(int dummy);
int procB(int dummy);
int procC(int dummy);
int tdb(int e);

void count();
void count2();
int Global;
char	c, d;
PROCESS	A_pid, B_pid, C_pid;
int argc;
char **argv;

main(myargc, myargv)
    int myargc;
    char **myargv; 
{
    int k;
    cthread_t	ct;
    PROCESS temp;
    struct  condition	mycond;
    struct  mutex	myMutex;

    argc = myargc;
    argv = myargv;
    Thread_blk_init();
    temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
    temp->ep = (PFI)tdb;
    temp->parm = 0;
    thread_ent[0].thread_pcb = temp;
    for (k=0; k<MAXPROC; k++){
	ct = cthread_fork((cthread_fn_t)Camelot_Thread_Rtn, (any_t) k);
	cthread_detach(ct);
    }
    condition_init(&mycond);
    mutex_init(&myMutex);
    condition_wait(&mycond, &myMutex);
}

int tdb(e)
    int e;
    {
    int a;
    char b;
    
    LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY+1, (PROCESS *) &a);
    IOMGR_Initialize();
    printf("Main thread going to create procA\n");
    LWP_CreateProcess((PFI)procA, 2048, LWP_NORMAL_PRIORITY, NULL, "procA", &A_pid);
/*  printf("Main thread going to create procB\n");
    LWP_CreateProcess((PFI)procB, 2048, LWP_NORMAL_PRIORITY, NULL, "procB", &B_pid); 
    printf("Main thread going to create procC \n");
    LWP_CreateProcess((PFI)procC, 2048, LWP_NORMAL_PRIORITY, NULL, "procC", &C_pid);
    printf("Main thread returned from creating procC  \n"); */
    LWP_WaitProcess(&b);


    }

procA(dummy)
    int dummy;
    {
    int    i;
    int    s;
    struct timeval t;
    int n;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	exit();
    }
    t.tv_sec = 5;
    t.tv_usec = 0;

    while(1)
	{
	count();
	Global = 1;
	printf("procA starting ...\n");
/*	printf("procA:please input a number");
	scanf("%d", &i); 
    	if (Global != 1) {
	    printf("Sorry - ProcA had the Global clobbered ... \n");
	}	
*/
	n = IOMGR_Select(1, 0, 0, 0, &t);
	printf("procA: select returned %d\n", n);
	if (n < 0)
	    perror("select");


/*	printf("procA waiting on d...\n");
	LWP_WaitProcess(&d); 
	printf("procA doing a no yield signal on c...\n");
	LWP_NoYieldSignal(&c);  
	printf("procA: going to wait via QWait \n");
	LWP_QWait();
	printf("procA just woken up from QWait\n"); */
	printf("procA yielding ...\n");
	LWP_DispatchProcess();
	}
    }

procB(dummy)
    int dummy;
    {
     int i;
    while(1)
	{
	count();
	Global = 2;
	printf("procB starting ...\n");
	cthread_yield();
	cthread_yield();
	cthread_yield();
	if (Global != 2) {
	    printf("Sorry - ProcB had the Global clobbered ... \n");
	}	
/*	printf("procB waiting on d...\n");
	LWP_WaitProcess(&d); */
	printf("procB waiting via QWait \n");
	LWP_QWait();
	printf("procB just woken up from QWait \n");
	}
    }

procC(dummy)
    int dummy;
    {
     int i;
    while(1)
	{
	count();
	Global = 3;
	printf("procC starting ...\n");
	printf("procC:please input a number");
	scanf("%d", &i); 
	printf("\n");
	if (Global != 3) {
	    printf("Sorry - ProcC had the Global clobbered ... \n");
	} 
/*	printf("procC doing a no yield signal on char d...\n");
	LWP_SignalProcess(&d); */
/*	printf("procC yielding by waiting for char c ...\n");
	LWP_WaitProcess(&c); */
/*	printf("procC: going to Qsignal procA \n");
	LWP_QSignal(A_pid); */
	LWP_DispatchProcess();
	printf("procC: going to Qsignal procB \n");
	LWP_QSignal(B_pid);
	}
    }

void count()
    {
    count2();
    }

void count2()
    {
    int i;
    for (i = 0; i < 10000; i++)getpid();
    }
