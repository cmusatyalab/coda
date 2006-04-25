/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <lwp/lwp.h>

void procA(void *dummy);
void procB(void *dummy);
void procC(void *dummy);
int tdb(int e);

void count();
void count2();
int Global;
char	c, d;
PROCESS	A_pid, B_pid, C_pid;

int main(int argc, char **argv)
{
    PROCESS mainthread;
    char b;

    LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY+1, &mainthread);
    IOMGR_Initialize();
    printf("Main thread going to create procA\n");
    LWP_CreateProcess(procA, 8192, LWP_NORMAL_PRIORITY, NULL, "procA", &A_pid);
    printf("Main thread going to create procB\n");
    LWP_CreateProcess(procB, 8192, LWP_NORMAL_PRIORITY, NULL, "procB", &B_pid); 
    printf("Main thread going to create procC \n");
    LWP_CreateProcess(procC, 8192, LWP_NORMAL_PRIORITY, NULL, "procC", &C_pid);
    printf("Main thread returned from creating procC  \n");
    LWP_WaitProcess(&b);
    exit(0);
}

void procA(void *dummy)
{
    int    i;
    struct timeval t;
    int n;

    t.tv_sec = 5;
    t.tv_usec = 0;

    while(1)
    {
        count();
        Global = 1;
        printf("procA starting ...\n");
        printf("procA:please input a number");
        scanf("%d", &i); 
        if (Global != 1) {
            printf("Sorry - ProcA had the Global clobbered ... \n");
        }	

        n = IOMGR_Select(1, 0, 0, 0, &t);
        printf("procA: select returned %d\n", n);
        if (n < 0)
            perror("select");


        //printf("procA waiting on d...\n");
        //LWP_WaitProcess(&d); 
        printf("procA doing a no yield signal on c...\n");
        LWP_NoYieldSignal(&c);  
        printf("procA: going to wait via QWait \n");
        LWP_QWait();
        printf("procA just woken up from QWait\n");
        printf("procA yielding ...\n");
        LWP_DispatchProcess();
    }
}

void procB(void *dummy)
{
    while(1)
    {
	count();
	Global = 2;
	printf("procB starting ...\n");
	if (Global != 2) {
	    printf("Sorry - ProcB had the Global clobbered ... \n");
	}	
	printf("procB waiting on d...\n");
	LWP_WaitProcess(&d);
	printf("procB waiting via QWait \n");
	LWP_QWait();
	printf("procB just woken up from QWait \n");
	}
    }

void procC(void *dummy)
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
        printf("procC doing a no yield signal on char d...\n");
        LWP_SignalProcess(&d); 
        printf("procC yielding by waiting for char c ...\n");
        LWP_WaitProcess(&c);
        printf("procC: going to Qsignal procA \n");
        LWP_QSignal(A_pid);
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
