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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/camstuff/test.cc,v 1.1 1996/11/22 19:09:10 braam Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/stat.h>
#include <camlib/camlib.h>
#include <cam/camelot_prefixed.h>
#include <camlib/camlib_prefixed.h>

#ifdef __cplusplus
}
#endif __cplusplus

#define ViceLog(n, fmt, msg)	printf(fmt, msg);
#define VFAIL 200
RvmType RvmMechanism = NotYetSet;
char *_Rvm_Log_Device;
char *_Rvm_Data_Device;
rvm_offset_t _Rvm_DataLength;
/*PRIVATE */char *cam_log_file;
/*PRIVATE */int camlog_fd;
/*PRIVATE */char camlog_record[512 + 8 + 1000];

CAMLIB_BEGIN_RECOVERABLE_DECLARATIONS
    int doggy[20];
    char kitty[100];
CAMLIB_END_RECOVERABLE_DECLARATIONS

PRIVATE int ParseArgs(int argc, char *argv[])
{
    int   i;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-nc")){
	    if (RvmMechanism != NotYetSet) {
		printf("Multiple Persistence methods selected.\n");
		exit(-1);
	    }
	    RvmMechanism = NoPersistence;
	    if (i < argc - 1) cam_log_file = argv[++i];
	}
	else
	    if (!strcmp(argv[i], "-cam")) {
		if (RvmMechanism != NotYetSet) {
		    printf("Multiple Persistence methods selected.\n");
		    exit(-1);
		}
		RvmMechanism = Camelot;
	    }
	else
	    if (!strcmp(argv[i], "-rvm")) {
		struct stat buf;
		if (RvmMechanism != NotYetSet) {
		    printf("Multiple Persistence methods selected.\n");
		    exit(-1);
		}

		if (i + 3 > argc) {	/* Need three arguments here */
		    printf("rvm needs 3 args: LOGDEV DATADEV DATA-LENGTH.\n");
		    exit(-1);
		}
		
		RvmMechanism = Rvm;
		_Rvm_Log_Device = (char *)malloc(strlen(argv[++i]));
		strcpy(_Rvm_Log_Device, argv[i]);
		_Rvm_Data_Device = (char *)malloc(strlen(argv[++i]));
		strcpy(_Rvm_Data_Device, argv[i]);
		if (stat(_Rvm_Log_Device, &buf) != 0) {
		    perror("Can't open Log Device");
		    exit(-1);
		}

		if (stat(_Rvm_Data_Device, &buf) != 0) {
		    perror("Can't open Data Device");
		    exit(-1);
		}
		_Rvm_DataLength = RVM_MK_OFFSET(0, atoi(argv[++i]));
	    }
	else {
	    return(-1);
	}
    }
    return(0);
}

main(int argc, char **argv)
{
    char *table[10];
    int camstatus;
    
    if (ParseArgs(argc, argv)) {
	printf("ParseArgs failed.\n");
	exit(-1);
    }
    
    switch (RvmMechanism) {
	case Camelot 	   : printf("RvmMechanism == Camelot\n"); break;
	case Rvm     	   : printf("RvmMechanism == Rvm\n"); break;
	case NoPersistence : printf("RvmMechanism == NoPersistence\n"); break;
	case NotYetSet	   : printf("No RvmMechanism selected!\n"); exit(-1);
    }
    CAMLIB_INITIALIZE_SERVER((CAM_PTF_CAMLIB_INITPROC_T) NULL, TRUE, "dog"); 

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	table[1] = CAMLIB_REC_MALLOC(100);
	table[2] = CAMLIB_REC_MALLOC(100);
	table[3] = CAMLIB_REC_MALLOC(100);
	table[4] = CAMLIB_REC_MALLOC(100);
	table[5] = CAMLIB_REC_MALLOC(100);

        CAMLIB_MODIFY(*table[1], 0x40);
        CAMLIB_MODIFY(*table[2], 0x50);
        CAMLIB_MODIFY(*table[3], 0x60);
        CAMLIB_MODIFY(*table[4], 0x70);
        CAMLIB_MODIFY(*table[5], 0x80);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, camstatus)
    if (camstatus)
	printf("First Transaction failed! %d\n", camstatus);
    
    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	CAMLIB_REC_FREE((char *)table[2]);
        CAMLIB_REC_FREE((char *)table[4]);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, camstatus)
    if (camstatus)
	printf("First Transaction failed! %d\n", camstatus);

    CAMLIB_BEGIN_TOP_LEVEL_TRANSACTION_2(CAM_TRAN_NV_SERVER_BASED)
	CAMLIB_REC_FREE((char *)table[1]);
        CAMLIB_REC_FREE((char *)table[3]);
        CAMLIB_ABORT(VFAIL);
    CAMLIB_END_TOP_LEVEL_TRANSACTION_2(CAM_PROT_TWO_PHASED, camstatus)	
    if (camstatus)
	printf("First Transaction failed! %d\n", camstatus);
}    
