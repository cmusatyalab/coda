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
                           none currently

#*/



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
    CODA_ASSERT(rvmlib_thread_data() != 0);

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
/* Do a partial vol package init.  VInitVolPackage() invokes the
 * salvager, we can't.
 */
void NortonInitVolPackage() 
{
    VInitServerList(NULL);
    InitLRU(VOLUMECACHESIZE);
    InitVolTable(HASHTABLESIZE);

    
    VInitVnodes(vLarge, 10);
    VInitVnodes(vSmall, 10);

    /* Initialize the resolution storage structures */
    //InitLogStorage();
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

