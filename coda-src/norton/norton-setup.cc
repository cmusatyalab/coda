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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/norton/norton-setup.cc,v 4.5 1998/09/15 14:27:55 jaharkes Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <lwp.h>
#include <rvm.h>
#include <rds.h>
#include <rvmlib.h>
#include <codadir.h>
#include <partition.h>
#include <util.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <rec_dlist.h>
#include <voltypes.h>
#include <volume.h>

#include "parser.h"
#include "norton.h"

#define NUMBUFS 20
#define VOLUMECACHESIZE 50

int norton_debug = 0;

struct camlib_recoverable_segment *camlibRecoverableSegment;

void LoadRVM(char * log_dev, char * data_dev, rvm_offset_t data_len) {
    rvm_return_t err;
    rvm_options_t * options;
    rvm_perthread_t *rvmptt;
    struct stat buf;

    if (stat(log_dev, &buf)) {
	perror("Reading log device");
	exit(1);
    }

    if (stat(data_dev, &buf)) {
	perror("Reading data device");
	exit(1);
    }

    /* Set the per-thread data structure, don't ever free this. */
    rvmptt = (rvm_perthread_t *)malloc(sizeof(rvm_perthread_t));
    rvmptt->tid = NULL;
    rvmptt->list.table = NULL;
    rvmptt->list.count = 0;
    rvmptt->list.size = 0;
    rvmlib_set_thread_data(rvmptt);
    assert(rvmlib_thread_data() != 0);

    options = rvm_malloc_options();
    options->log_dev = log_dev;
    options->flags = 0;

    // Supress truncation, do it manually if needed.
    options->truncate = 0;

    printf("About to call RVM_INIT\n");
    fflush(stdout);
    if ((err = RVM_INIT(options)) != RVM_SUCCESS) {
	fprintf(stderr, "rvm_init failed %s\n", rvm_return(err));
	exit(1);
    }

    printf("About to call rds_load_heap\n");
    fflush(stdout);
    rds_load_heap(data_dev, data_len,
		  (char **)&camlibRecoverableSegment,
		  (int *)&err); 
    if (err != RVM_SUCCESS) {
	fprintf(stderr, "rds_load_heap error %s\n", rvm_return(err));
	exit(1);
    }

    rvm_free_options(options);
}



/* Initialize LWP */
void InitLWP() {
    PROCESS	pid;
    int		ret;
    
    ret = LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &pid);
    if (ret != LWP_SUCCESS) {
	fprintf(stderr, "LWP_Init failed!\n");
	exit(1);
    }

    /* Initialize IOMGR!  BAH!  LWP_Init should do this! */
    ret = IOMGR_Initialize();
    if (ret != LWP_SUCCESS) {
	fprintf(stderr, "IOMGR_Initialize failed!  Return = %d\n", ret);
	exit(1);
    }
}



extern void InitVolTable(int);
extern void InitLogStorage();
extern struct DiskPartition *DiskPartitionList;
/* Do a partial vol package init.  VInitVolPackage() invokes the
 * salvager, we can't.
 */
void NortonInitVolPackage() 
{

    DiskPartitionList = NULL;

    VInitServerList();
    InitLRU(VOLUMECACHESIZE);
    InitVolTable(HASHTABLESIZE);

    
    VInitVnodes(vLarge, 10);
    VInitVnodes(vSmall, 10);

    /* Initialize the resolution storage structures */
    InitLogStorage();
    
}


void NortonInit(char *log_dev, char *data_dev, int data_len) {
    struct stat		buf;
    ProgramType		pt;

    /* initialize rvmlib code */
    RvmType = RAWIO;
    
    
    /* Make sure there isn't a server already running */
    if (stat("/vice/srv/pid", &buf) == 0) {
	fprintf(stderr, "ERROR: A file server is already running!\n");
	fprintf(stderr, "       Shut it down before running 'norton'\n");
	exit(1);
    }

    InitLWP();
    LoadRVM(log_dev, data_dev, RVM_MK_OFFSET(0, data_len));

    DIR_Init(DIR_DATA_IN_VM);
    DC_HashInit();

    // Pretend we are the salvager.
    pt = salvager;
    if (LWP_NewRock(FSTAG, (char *)&pt) != LWP_SUCCESS) {
	fprintf(stderr, "Can't set program type!\n");
	exit(1);
    }
    
    NortonInitVolPackage();
}

