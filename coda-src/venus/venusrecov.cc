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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/venusrecov.cc,v 4.6 1997/12/20 00:09:19 braam Exp $";
#endif /*_BLURB_*/





/*
 *
 *    Implementation of the Venus Recoverable Storage manager.
 *
 *
 */


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#include <mach.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#ifdef	__linux__
#define MAX(a,b)   ( (a) > (b) ? (a) : (b))
#endif

/* from rvm */
#include <rds.h>
#include <rvm.h>
#include <rvm_segment.h>
#include <rvm_statistics.h>

#if defined(__linux__) || defined(__BSD44__)
#include <sys/mman.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include "fso.h"
#include "hdb.h"
#include "local.h"
#include "mariner.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "worker.h"

extern char *rvm_errmsg;


/*  *****  Exported Variables  *****  */

int RecovInited = 0;
RecovVenusGlobals *rvg = 0;
int TransCount = 0;
float TransElapsed = 0.0;

int InitMetaData = UNSET_IMD;
char *VenusLogDevice = UNSET_VLD;
unsigned long VenusLogDeviceSize = UNSET_VLDS;
char *VenusDataDevice = UNSET_VDD;
unsigned long VenusDataDeviceSize = UNSET_VDDS;
int RdsChunkSize = UNSET_RDSCS;
int RdsNlists = UNSET_RDSNL;
int CMFP = UNSET_CMFP;
int DMFP = UNSET_DMFP;
int MAXFP = UNSET_MAXFP;
int WITT = UNSET_WITT;
int MAXFS = UNSET_MAXFS;
int MAXTS = UNSET_MAXTS;


/*  *****  Private Constants  *****  */

#ifdef MACH
PRIVATE const char *VM_RVGADDR = (char *)0x00c00000;
PRIVATE const char *VM_RDSADDR = (char *)0x01c00000;
#elif defined(NetBSD1_3)
PRIVATE const char *VM_RVGADDR = (char *)0x50000000;
PRIVATE const char *VM_RDSADDR = (char *)0x51000000;
#elif defined(__BSD44__)
PRIVATE const char *VM_RVGADDR = (char *)0x40000000;
PRIVATE const char *VM_RDSADDR = (char *)0x41000000;
#elif	defined(__linux__) || defined(__CYGWIN32__)
PRIVATE const char *VM_RVGADDR = (char *)0x20000000;
PRIVATE const char *VM_RDSADDR = (char *)0x21000000;
#endif


/*  *****  Private Variables  *****  */

PRIVATE rvm_options_t Recov_Options;
PRIVATE char *Recov_RvgAddr = 0;
PRIVATE rvm_length_t Recov_RvgLength = 0;
PRIVATE char *Recov_RdsAddr = 0;
PRIVATE rvm_length_t Recov_RdsLength = 0;
PRIVATE int Recov_TimeToFlush = 0;
PRIVATE rvm_statistics_t Recov_Statistics;

/*  *****  Private Functions  *****  */

PRIVATE void Recov_CheckParms();
PRIVATE void Recov_InitRVM();
PRIVATE void Recov_LayoutSeg();
PRIVATE void Recov_CreateSeg();
PRIVATE void Recov_LoadSeg();
PRIVATE void Recov_InitSeg();
PRIVATE void Recov_GetStatistics();
PRIVATE void Recov_AllocateVM(char **, unsigned long);
PRIVATE void Recov_DeallocateVM(char *, unsigned long);

/* Crude formula for estimating recoverable data requirements! */
#define	RECOV_BYTES_NEEDED()\
    (MLEs * (sizeof(cmlent) + 64) +\
    CacheFiles * (sizeof(fsobj) + 64) +\
    CacheFiles / 4 * (sizeof(VenusDirData) + 3072) +\
    CacheFiles / 64 * sizeof(volent) +\
    CacheFiles / 256 * sizeof(vsgent) +\
    HDBEs * (sizeof(hdbent) + 128) +\
    128 * 1024)


/*  *****  Recovery Module  *****  */

int RecovVenusGlobals::validate() {
    if (recov_MagicNumber != RecovMagicNumber) return(0);
    if (recov_VersionNumber != RecovVersionNumber) return(0);

    if (recov_CleanShutDown != 0 && recov_CleanShutDown != 1) return(0);

    if (!VALID_REC_PTR(recov_FSDB)) return(0);
    if (!VALID_REC_PTR(recov_VDB)) return(0);
    if (!VALID_REC_PTR(recov_VSGDB)) return(0);
    if (!VALID_REC_PTR(recov_HDB)) return(0);
    if (!VALID_REC_PTR(recov_LRDB)) return(0);
/*    if (!VALID_REC_PTR(recov_UDB)) return(0);*/
/*    if (!VALID_REC_PTR(recov_VMDB)) return(0);*/
    if (!VALID_REC_PTR(recov_VCBDB)) return(0);

    return(1);
}


void RecovVenusGlobals::print() {
    print(stdout);
}


void RecovVenusGlobals::print(FILE *fp) {
    print(fileno(fp));
}


/* local-repair modification */
void RecovVenusGlobals::print(int fd) {
    fdprint(fd, "RVG values: what they are (what they should be)\n");
    fdprint(fd, "Magic = %x(%x), Version = %d(%d), CleanShutDown= %d(0 or 1), RootVolName = %s\n",
	    recov_MagicNumber, RecovMagicNumber,
	    recov_VersionNumber, RecovVersionNumber,
	    recov_CleanShutDown, recov_RootVolName);
    fdprint(fd, "The following pointers should be between %x and %x:\n",
	    recov_HeapAddr, recov_HeapAddr + recov_HeapLength);
    fdprint(fd, "Ptrs = [%x %x %x %x %x %x], Heap = [%x] HeapLen = %x\n",
	     recov_FSDB, recov_VDB, recov_VSGDB, recov_HDB, recov_LRDB, recov_VCBDB, 
	     recov_HeapAddr, recov_HeapLength);
}


void RecovInit() {
    /* Set unset parameters to defaults (as appropriate). */
    Recov_CheckParms();

    if (RvmType == VM) {
	if ((rvg = (RecovVenusGlobals *)malloc(sizeof(RecovVenusGlobals))) == 0)
	    Choke("RecovInit: malloc failed");
	bzero((void *)rvg, (int)sizeof(RecovVenusGlobals));
	rvg->recov_MagicNumber = RecovMagicNumber;
	rvg->recov_VersionNumber = RecovVersionNumber;
	rvg->recov_LastInit = Vtime();
	RecovInited = 1;
	return;
    }

    /* Initialize the RVM package. */
    Recov_InitRVM();

    /* Layout the regions (i.e., specify their <address, length>) comprising the Venus meta-data segment. */
    Recov_LayoutSeg();

    /* Brain-wipe the segment if requested from command-line. */
    if (InitMetaData) {
	eprint("brain-wiping recoverable store");
	Recov_CreateSeg();
    }

    /* Load the segment, and initialize/validate it. */
    eprint("loading recoverable store");
    Recov_LoadSeg();
    Recov_InitSeg();

    /* Read-in bounds for bounded recoverable data structures. */
    if (!InitMetaData) {
	MLEs = VDB->MaxMLEs;
	CacheFiles = FSDB->MaxFiles;
	HDBEs = HDB->MaxHDBEs;
	LOG(10, ("RecovInit: MLEs = %d, CacheFiles = %d, HDBEs = %d\n",
		 MLEs, CacheFiles, HDBEs));
    }

    RecovInited = 1;

    /* Fire up the daemon. */
    RECOVD_Init();
}


PRIVATE void Recov_CheckParms() {
    /* From recov module. */
    {
	if (InitMetaData == UNSET_IMD) InitMetaData = DFLT_IMD;
	if (RvmType == UNSET) RvmType = DFLT_RVMT;
	switch(RvmType) {
	    case RAWIO:
		{
		eprint("RAWIO not yet supported");
		exit(-1);

		break;
		}

	    case UFS:
	    case VM:
		{
		break;
		}

	    case UNSET:
	    default:
		Choke("Recov_CheckParms: bogus RvmType (%d)", RvmType);
	}

	/* VM RvmType forces a brain-wipe! */
	if (RvmType == VM) InitMetaData = 1;

	/* Specifying log or data size requires a brain-wipe! */
	if (VenusLogDevice == UNSET_VLD) VenusLogDevice = DFLT_VLD;
	if (VenusLogDeviceSize != UNSET_VLDS && !InitMetaData)
	    { eprint("setting VLDS requires InitMetaData"); exit(-1); }
	if (VenusDataDevice == UNSET_VDD) VenusDataDevice = DFLT_VDD;
	if (VenusDataDeviceSize != UNSET_VDDS && !InitMetaData)
	    { eprint("setting VDDS requires InitMetaData"); exit(-1); }

	/* Specifying either RDS chunk size or nlists requires a brain-wipe! */
	/* These parameters are only needed for a brain-wipe anyway! */
	if (RdsChunkSize != UNSET_RDSCS && !InitMetaData)
	    { eprint("setting RDS chunk size requires InitMetaData"); exit(-1); }
	if (RdsChunkSize == UNSET_RDSCS) RdsChunkSize = DFLT_RDSCS;
	if (RdsNlists != UNSET_RDSNL && !InitMetaData)
	    { eprint("setting RDS nlists requires InitMetaData"); exit(-1); }
	if (RdsNlists == UNSET_RDSNL) RdsNlists = DFLT_RDSNL;

	/* Flush/Truncate parameters. */
	if (CMFP == UNSET_CMFP) CMFP = DFLT_CMFP;
	if (DMFP == UNSET_DMFP) DMFP = DFLT_DMFP;
	if (MAXFP == UNSET_MAXFP) MAXFP = DFLT_MAXFP;
	if (WITT == UNSET_WITT) WITT = DFLT_WITT;
	if (MAXFS == UNSET_MAXFS) MAXFS = DFLT_MAXFS;
	if (MAXTS == UNSET_MAXTS) MAXTS = DFLT_MAXTS;
    }

    /* From VDB module. */
    {
	/* Specifying MLEs requires a brain-wipe! */
	if (MLEs != UNSET_MLE && !InitMetaData)
	    { eprint("setting MLEs requires InitMetaData"); exit(-1); }

	/* MLEs is not set to default unless brain-wipe has been requested! */
	if (MLEs == UNSET_MLE && InitMetaData)
	    MLEs = CacheBlocks / BLOCKS_PER_MLE;
	if (MLEs != UNSET_MLE && MLEs < MIN_MLE) {
	    eprint("minimum MLEs is %d", MIN_MLE); 
	    eprint("Cannot start. Use more than %d MLEs", MIN_MLE);
	    exit(-1); 
	}
    }

    /* From FSDB module. */
    {
	/* Specifying CacheFiles requires a brain-wipe! */
	if (CacheFiles != UNSET_CF && !InitMetaData)
	    { eprint("setting CacheFiles requires InitMetaData"); exit(-1); }

	/* CacheFiles is not set to default unless brain-wipe has been requested! */
	if (CacheFiles == UNSET_CF && InitMetaData)
	    CacheFiles = CacheBlocks / BLOCKS_PER_FILE;
	if (CacheFiles != UNSET_CF && CacheFiles < MIN_CF) {
	    eprint("Cannot start: minimum cache files is %d", MIN_CF); 
	    exit(-1); 
	}
    }

    /* From HDB module. */
    {
	/* Specifying HDBEs requires a brain-wipe! */
	if (HDBEs != UNSET_HDBE && !InitMetaData)
	    { eprint("setting HDBEs requires InitMetaData"); exit(-1); }

	/* HDBEs is not set to default unless brain-wipe has been requested! */
	if (HDBEs == UNSET_HDBE && InitMetaData)
	    HDBEs = CacheBlocks / BLOCKS_PER_HDBE;
	if (HDBEs != UNSET_HDBE && HDBEs < MIN_HDBE) {
	    eprint("Cannot start. Minimum HDBEs is %d", MIN_HDBE); 
	    exit(-1); 
	}
    }
}


PRIVATE void Recov_InitRVM() {

    rvm_init_options(&Recov_Options);
    Recov_Options.log_dev = VenusLogDevice;
    Recov_Options.truncate = 0;
    Recov_Options.flags = RVM_COALESCE_TRANS;  /* oooh, daring */

    rvm_init_statistics(&Recov_Statistics);

    if (InitMetaData) {
	/* Compute recoverable storage requirements, and verify that log/data sizes are adequate. */
	{
	    int RecovBytesNeeded = (int) RECOV_BYTES_NEEDED();

	    /* Set segment sizes if necessary. */
	    if (VenusDataDeviceSize == UNSET_VDDS)
		VenusDataDeviceSize = RecovBytesNeeded;
	    if (VenusLogDeviceSize == UNSET_VLDS)
		VenusLogDeviceSize = VenusDataDeviceSize / DataToLogSizeRatio;

	    /* Check that sizes meet minimums. */
	    if (VenusLogDeviceSize < MIN_VLDS) {
		eprint("log segment too small (%#x); minimum %#x",
		       VenusLogDeviceSize, MIN_VLDS);
		exit(-1);
	    }
	    if (VenusDataDeviceSize < MAX(RecovBytesNeeded, MIN_VDDS)) {
		eprint("data segment too small (%#x); minimum %#x",
		       VenusDataDeviceSize, MAX(RecovBytesNeeded, MIN_VDDS));
		exit(-1);
	    }

	    LOG(0, ("RecovDataSizes: Log = %#x, Data = %#x\n",
		    VenusLogDeviceSize, VenusDataDeviceSize));
	}

	/* Initialize log and data segment. */
	{
	    unlink(VenusLogDevice);
	    {
		/* Log initialization must be done by another process, since an RVM_INIT */
		/* with NULL log must be specified (to prevent RVM from trying to do recovery). */
		int child = fork();
		if (child == -1)
		    Choke("Recov_InitRVM: fork failed (%d)", errno);
		if (child == 0) {
		    rvm_return_t ret = RVM_INIT(NULL);
		    if (ret != RVM_SUCCESS) {
			if (ret == RVM_EINTERNAL)
			    eprint("Recov_InitRVM(child): RVM_INIT failed, internal error %s",
				   rvm_errmsg);
			else
			    eprint("Recov_InitRVM(child): RVM_INIT failed (%d)", ret);
			exit(ret);
		    }

		    rvm_offset_t logsize = RVM_MK_OFFSET(0, VenusLogDeviceSize);
		    ret = rvm_create_log(&Recov_Options, &logsize, 0600);
		    if (ret != RVM_SUCCESS) {
			eprint("Recov_InitRVM(child): rvm_create_log failed (%d)", ret);
			exit(ret);
		    }

		    exit(RVM_SUCCESS);
		}
		else {
		    union wait status;
#ifdef __MACH__
		    int exiter = wait(&status);
#else
		    int exiter = wait(&status.w_status);
#endif /* __linux__ ||__BSD44__ */
		    if (exiter != child)
			Choke("Recov_InitRVM: exiter (%d) != child (%d)", exiter, child);
#ifdef __MACH__
		    if (status.w_retcode != RVM_SUCCESS)
			Choke("Recov_InitRVM: log initialization failed (%d)", status.w_retcode);
#endif /* __MACH__ */
#ifdef __BSD44__
		    if (WEXITSTATUS(status.w_status) != RVM_SUCCESS)
			Choke("Recov_InitRVM: log initialization failed (%d)", WEXITSTATUS(status.w_status));
#endif /* __BSD44__ */

		}
	    }
	    eprint("%s initialized at size %#x",
		   VenusLogDevice, VenusLogDeviceSize);

	    int fd = 0;
	    if ((fd = open(VenusDataDevice, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
		Choke("Recov_InitRVM: create of (%s) failed (%d)",
		    VenusDataDevice, errno);
	    {
		const int ID_BLKSIZE = 4096;
		char buf[ID_BLKSIZE];
		bzero((void *)buf, ID_BLKSIZE);

		int nblocks = (int) VenusDataDeviceSize / ID_BLKSIZE;
		for (int i = 0; i < nblocks; i++)
		    if (write(fd, buf, ID_BLKSIZE) != ID_BLKSIZE)
			Choke("Recov_InitRVM: write on (%s) failed (%d)",
			    VenusDataDevice, errno);

		int remainder = (int) VenusDataDeviceSize % ID_BLKSIZE;
		if (remainder != 0)
		    if (write(fd, buf, remainder) != remainder)
			Choke("Recov_InitRVM: write on (%s) failed (%d)",
			    VenusDataDevice, errno);
	    }
	    if (close(fd) < 0)
		Choke("Recov_InitRVM: close of (%s) failed (%d)",
		    VenusDataDevice, errno);
	    eprint("%s initialized at size %#x",
		   VenusDataDevice, VenusDataDeviceSize);
	}
    }
    else {
	/* Validate log and data segment. */
	{
	    struct stat tstat;

	    if (stat(VenusLogDevice, &tstat) < 0)
		Choke("ValidateDevice: stat of (%s) failed (%d)",
		    VenusLogDevice, errno);
	    VenusLogDeviceSize = tstat.st_size;
	    eprint("%s validated at size %#x",
		   VenusLogDevice, VenusLogDeviceSize);

	    if (stat(VenusDataDevice, &tstat) < 0)
		Choke("ValidateDevice: stat of (%s) failed (%d)",
		    VenusDataDevice, errno);
	    VenusDataDeviceSize = tstat.st_size;
	    eprint("%s validated at size %#x",
		   VenusDataDevice, VenusDataDeviceSize);
	}
    }

    rvm_return_t ret = RVM_INIT(&Recov_Options);
    if (ret == RVM_ELOG_VERSION_SKEW) {
	eprint("Recov_InitRVM: RVM_INIT failed, RVM log version skew");
	eprint("Venus not started");
	exit(-1);
    } else if (ret == RVM_EINTERNAL)
	Choke("Recov_InitRVM: RVM_INIT failed, internal error %s", rvm_errmsg);
    else if (ret != RVM_SUCCESS)
	Choke("Recov_InitRVM: RVM_INIT failed (%d)", ret);

}


/* Venus meta-data segment consists of two regions, RVG and RDS. */
PRIVATE void Recov_LayoutSeg() {
    /* RVG region: structure containing small number of global variables. */
    Recov_RvgAddr = (char *)VM_RVGADDR;
    Recov_RvgLength = RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE(sizeof(RecovVenusGlobals));

    /* RDS region: recoverable heap. */
    Recov_RdsAddr = (char *)VM_RDSADDR;
    rvm_length_t Recov_Margin =	4 * RVM_PAGE_SIZE;	/* safety factor */
    Recov_RdsLength = RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE(VenusDataDeviceSize) -
      Recov_RvgLength - Recov_Margin;
}


/* Might need to fork here! -JJK */
PRIVATE void Recov_CreateSeg() {
    /* The Venus segment has two regions. */
    /* N.B. The segment package assumes that VM is allocated PRIOR to seg creation! */
    unsigned long nregions = 2;
    rvm_region_def_t regions[2];

    /* Region 0 is the block of recoverable Venus globals. */
    rvm_offset_t rvg_offset; RVM_ZERO_OFFSET(rvg_offset);
    Recov_AllocateVM(&Recov_RvgAddr, (unsigned long)Recov_RvgLength);
    RVM_INIT_REGION(regions[0], rvg_offset, Recov_RvgLength, Recov_RvgAddr);

    /* Region 1 is the recoverable heap. */
    rvm_offset_t rds_offset = RVM_ADD_LENGTH_TO_OFFSET(rvg_offset, Recov_RvgLength);
    Recov_AllocateVM(&Recov_RdsAddr, (unsigned long)Recov_RdsLength);
    RVM_INIT_REGION(regions[1], rds_offset, Recov_RdsLength, Recov_RdsAddr);

    LOG(10, ("Recov_CreateSeg: RVG = (%x, %x), RDS = (%x, %x)\n",
	      Recov_RvgAddr, Recov_RvgLength, Recov_RdsAddr, Recov_RdsLength));
    rvm_offset_t dummy;
    RVM_ZERO_OFFSET(dummy);	/* VenusDataDevice is file, must zero dummy */
    rvm_return_t ret = rvm_create_segment(VenusDataDevice, dummy,
					   &Recov_Options, nregions, regions);
    if (ret != RVM_SUCCESS)
	Choke("Recov_CreateSeg: rvm_create_segment failed (%d)", ret);

    /* Work around RVM bug! */
    RecovFlush();
    RecovTruncate();

    /* Deallocate VM for regions since LoadSeg will reallocate it! */
    Recov_DeallocateVM(Recov_RvgAddr, Recov_RvgLength);
    Recov_DeallocateVM(Recov_RdsAddr, Recov_RdsLength);
}


PRIVATE void Recov_LoadSeg() {
    rvm_offset_t dummy;
    unsigned long nregions = 0;
    rvm_region_def_t *regions = 0;
    rvm_return_t ret = rvm_load_segment(VenusDataDevice, dummy,
					 &Recov_Options, &nregions, &regions);
    if (ret != RVM_SUCCESS)
	Choke("Recov_LoadSeg: rvm_load_segment failed (%d)", ret);

    /* Basic sanity checking. */
    if (nregions != 2)
	Choke("Recov_LoadSeg: bogus nregions (%d)", nregions);
    Recov_RvgAddr = regions[0].vmaddr;
    if (regions[0].length != Recov_RvgLength)
	Choke("Recov_LoadSeg: bogus regions[0] length (%x, %x)",
	    regions[0].length, Recov_RvgLength);
    Recov_RdsAddr = regions[1].vmaddr;
    if (regions[1].length != Recov_RdsLength)
	Choke("Recov_LoadSeg: bogus regions[1] length (%x, %x)",
	    regions[1].length, Recov_RdsLength);

    free(regions);

    LOG(10, ("Recov_LoadSeg: RVG = (%x, %x), RDS = (%x, %x)\n",
	      Recov_RvgAddr, Recov_RvgLength, Recov_RdsAddr, Recov_RdsLength));
}


PRIVATE void Recov_InitSeg() {
    rvg = (RecovVenusGlobals *)Recov_RvgAddr;

    /* Initialize/validate the segment. */
    if (InitMetaData) {
	TRANSACTION(
	    /* Initialize the block of recoverable Venus globals. */
	    RVMLIB_REC_OBJECT(*rvg);
	    bzero((void *)rvg, (int)sizeof(RecovVenusGlobals));
	    rvg->recov_MagicNumber = RecovMagicNumber;
	    rvg->recov_VersionNumber = RecovVersionNumber;
	    rvg->recov_LastInit = Vtime();
	    rvg->recov_HeapAddr = Recov_RdsAddr;
	    rvg->recov_HeapLength = (unsigned int)Recov_RdsLength;

	    /* Initialize the recoverable heap. */
	    int err = 0;
	    rds_init_heap(Recov_RdsAddr, Recov_RdsLength, (unsigned long)RdsChunkSize,
			  (unsigned long)RdsNlists, _rvm_data->tid, &err);
	    if (err != SUCCESS)
		Choke("Recov_InitSeg: rds_init_heap failed (%d)", err);
	)
    }
    else {
	/* Sanity check RVG fields. */
	if (rvg->recov_HeapAddr != Recov_RdsAddr || rvg->recov_HeapLength != Recov_RdsLength)
	    Choke("Recov_InitSeg: heap mismatch (%x, %x) vs (%x, %x)",
		rvg->recov_HeapAddr, rvg->recov_HeapLength, Recov_RdsAddr, Recov_RdsLength);
	if (!rvg->validate())
	    { rvg->print(stderr); Choke("Recov_InitSeg: rvg validation failed, trying restarting venus with -init"); }
#ifdef __linux__  /* strtok broken on Linux ? */
	eprint("Last init was %s\n", ctime((long *)&rvg->recov_LastInit));
#else
        eprint("Last init was %s", strtok(ctime((long *)&rvg->recov_LastInit), "\n"));
#endif

	/* Copy CleanShutDown to VM global, then set it FALSE. */
	CleanShutDown = rvg->recov_CleanShutDown;
	eprint("Last shutdown was %s", (CleanShutDown ? "clean" : "dirty"));
	TRANSACTION(
	    RVMLIB_REC_OBJECT(rvg->recov_CleanShutDown);
	    rvg->recov_CleanShutDown = 0;
	)
    }
/*
    eprint("Recov_InitSeg: magic = %x, version = %d, clean = %d, rvn = %s",
	    rvg->recov_MagicNumber, rvg->recov_VersionNumber,
	    rvg->recov_CleanShutDown, rvg->recov_RootVolName);
*/

    /* Fire up the recoverable heap. */
    {
	int err = 0;
	rds_start_heap((char *)Recov_RdsAddr, &err);
	if (err != 0)
	    Choke("Recov_InitSeg: rds_start_heap failed (%d)", err);

	/* Plumb the heap here? */
	if (MallocTrace) {	
	  rds_trace_on(rds_printer);
	  rds_trace_dump_heap();
	}
    }
}


/* Bounds the (non)persistence of committed no_flush transactions. */
void RecovSetBound(int bound) {
    if (bound < Recov_TimeToFlush)
	Recov_TimeToFlush = bound;
}


PRIVATE void Recov_GetStatistics() {
    if (RvmType == VM) return;

    rvm_return_t ret = RVM_STATISTICS(&Recov_Statistics);
    if (ret != RVM_SUCCESS)
	Choke("Recov_GetStatistics: rvm_statistics failed (%d)", ret);
}


void RecovFlush(int Force) {
    if (RvmType == VM) return;

    Recov_GetStatistics();
    int FlushCount = (int)Recov_Statistics.n_no_flush;
    unsigned long FlushSize = RVM_OFFSET_TO_LENGTH(Recov_Statistics.no_flush_length);

    char *reason = (Force) ? "F" :
      (Recov_TimeToFlush <= 0) ? "T" :
      (FlushSize >= MAXFS) ? "S" : "I";

    Recov_TimeToFlush = MAXFP;
    if (FlushSize == 0) return;

    MarinerLog("cache::BeginRvmFlush (%d, %d, %s)\n", FlushCount, FlushSize, reason);
    int pre_vm_usage = VMUsage();
    START_TIMING();
    rvm_return_t ret = rvm_flush();
    if (ret != RVM_SUCCESS)
	Choke("RecovFlush: rvm_flush failed (%d)", ret);
    END_TIMING();
    int post_vm_usage = VMUsage();
    MarinerLog("cache::EndRvmFlush\n");

/*    if (post_vm_usage - pre_vm_usage != 0)*/
    LOG(1, ("RecovFlush: count = %d, size = %d, elapsed = %3.1f, delta_vm = %x\n",
	    FlushCount, FlushSize, elapsed, post_vm_usage - pre_vm_usage));
}


void RecovTruncate(int Force) {
    if (RvmType == VM) return;

    Recov_GetStatistics();
    int TruncateCount = (int)Recov_Statistics.n_flush_commit +
      (int)Recov_Statistics.n_no_flush_commit;
    unsigned long TruncateSize = RVM_OFFSET_TO_LENGTH(Recov_Statistics.log_written);

    char *reason = (Force) ? "F" :
      (TruncateSize >= MAXTS) ? "S" : "I";

    if (TruncateSize == 0) return;

    MarinerLog("cache::BeginRvmTruncate (%d, %d, %s)\n", TruncateCount, TruncateSize, reason);
    int pre_vm_usage = VMUsage();
    START_TIMING();
    rvm_return_t ret = rvm_truncate();
    if (ret != RVM_SUCCESS)
	Choke("RecovTruncate: rvm_truncate failed (%d)", ret);
    END_TIMING();
    int post_vm_usage = VMUsage();
    MarinerLog("cache::EndRvmTruncate\n");

/*    if (post_vm_usage - pre_vm_usage != 0)*/
    LOG(1, ("RecovTruncate: count = %d, size = %d, elapsed = %3.1f, delta_vm = %x\n",
	    TruncateCount, TruncateSize, elapsed, post_vm_usage - pre_vm_usage));
}


void RecovTerminate() {
    if (RvmType == VM) return;
    if (!RecovInited) return;

    /* Record clean shutdown indication if possible. */
    Recov_GetStatistics();
    int n_uncommit = (int)Recov_Statistics.n_uncommit;
    if (n_uncommit == 0) {
	/* N.B.  Can't use rvmlib macros here, since we're likely being called in the */
	/* context of a signal handler, which does not have the state assumed by the macros! */
	{
	    rvm_tid_t tid;
	    rvm_init_tid(&tid);
	    rvm_return_t ret;
	    ret = rvm_begin_transaction(&tid, no_restore);
	    ASSERT(ret == RVM_SUCCESS);
	    ret = rvm_set_range(&tid, (char *)&rvg->recov_CleanShutDown,
				sizeof(rvg->recov_CleanShutDown));
	    ASSERT(ret == RVM_SUCCESS);
	    rvg->recov_CleanShutDown = 1;
	    ret = rvm_end_transaction(&tid, flush);
	    ASSERT(ret == RVM_SUCCESS);
	}

	eprint("RecovTerminate: clean shutdown");
    }
    else {
	eprint("RecovTerminate: dirty shutdown (%d uncommitted transactions)", n_uncommit);
    }

    rvm_return_t ret = rvm_terminate();
    switch(ret) {
	case RVM_SUCCESS:
	    ASSERT(n_uncommit == 0);
	    break;

	case RVM_EUNCOMMIT:
	    ASSERT(n_uncommit != 0);
	    break;

	default:
	    Choke("RecovTerminate: rvm_terminate failed (%d)", ret);
    }
}


void RecovPrint(int fd) {
    if (RvmType == VM) return;
    if (!RecovInited) return;

    fdprint(fd, "Recoverable Storage: (%s, %x)\n", VenusDataDevice, VenusDataDeviceSize);
    fdprint(fd, "\tTransactions = (%d, %3.1f)\n",
	     TransCount, (TransCount > 0 ? TransElapsed / TransCount : 0.0));
    fdprint(fd, "\tHeap: chunks = %d, nlists = %d, bytes = (%d, %d)\n",
	     RdsChunkSize, RdsNlists, 0, 0);
    fdprint(fd, "\tLast initialized %s\n", ctime((long *)&rvg->recov_LastInit));

    fdprint(fd, "***RVM Statistics***\n");
    Recov_GetStatistics();
    rvm_return_t ret = rvm_print_statistics(&Recov_Statistics, logFile);
    fflush(logFile);
    if (ret != RVM_SUCCESS)
	Choke("Recov_PrintStatistics: rvm_print_statistics failed (%d)", ret);

    fdprint(fd, "***RDS Statistics***\n");
    rds_stats_t rdsstats;
    if (rds_get_stats(&rdsstats) != 0)
	fdprint(fd, "rds_get_stats failed\n\n");
    else
        fdprint(fd, "RecovPrint:  Free bytes in heap = %d; Malloc'd bytes in heap = %d\n\n", 
	        rdsstats.freebytes, rdsstats.mallocbytes);
    // We wish there were a way to find out if heap_header_t.maxlist < heap_header_t.nlists.
    // If there were, that would be a sign that fragmentation is becoming a problem.
    // Unfortunately, heap_header_t is in rds_private.h
}


/*  *****  VM Allocation/Deallocation  *****  */

PRIVATE void Recov_AllocateVM(char **addr, unsigned long length) {
#if	MACH
    kern_return_t ret = vm_allocate(task_self(), (vm_address_t *)addr,
				    (unsigned int)length, (*addr == 0));
    if (ret != KERN_SUCCESS)
	Choke("Recov_AllocateVM: allocate(%x, %x) failed (%d)", *addr, length, ret);
    LOG(0, ("Recov_AllocateVM: allocated %x bytes at %x\n", length, *addr));

#elif defined(__linux__) || defined(__BSD44__)
    char *requested_addr = *addr;
    *addr = mmap(*addr, length, (PROT_READ | PROT_WRITE),
		 (MAP_PRIVATE | MAP_ANON), -1, 0);

    if (*addr == (char *)-1) {
	if (errno == ENOMEM)
	    Choke("Recov_AllocateVM: mmap(%x, %x, ...) out of memory", *addr, length);
	else
	    Choke("Recov_AllocateVM: mmap(%x, %x, ...) failed with errno == %d", *addr, length, errno);
    }

    if ((requested_addr != 0) && (*addr != requested_addr)) {
    	Choke("Recov_AllocateVM: mmap address mismatch; requested %x, returned %x", requested_addr, *addr);
    }

    LOG(0, ("Recov_AllocateVM: allocated %x bytes at %x\n", length, *addr));

#else /* DEFAULT */
    Choke("Recov_AllocateVM: not yet implemented for this platform!");
#endif
}


PRIVATE void Recov_DeallocateVM(char *addr, unsigned long length) {
#if	MACH
    kern_return_t ret = vm_deallocate(task_self(), (vm_address_t)addr, (unsigned int)length);
    if (ret != KERN_SUCCESS)
	Choke("Recov_DeallocateVM: deallocate(%x, %x) failed (%d)", addr, length, ret);
    LOG(0, ("Recov_DeallocateVM: deallocated %x bytes at %x\n", length, addr));
#elif defined(__linux__) || defined(__BSD44__)
    if (munmap(addr, length)) {
	Choke("Recov_DeallocateVM: munmap(%x, %x) failed with errno == %d", addr, length, errno);
    }
    LOG(0, ("Recov_DeallocateVM: deallocated %x bytes at %x\n", length, addr));
#else	/* MACH || __linux__ || __BSD44__ */
    Choke("Recov_DeallocateVM: not yet implemented for this platform");
#endif	/* MACH || __linux__ || __BSD44__ */
}


/*  *****  RVM String Routines  *****  */

RPC2_String Copy_RPC2_String(RPC2_String& src) {
    int len = (int) strlen((char *)src) + 1;

    RPC2_String tgt = (RPC2_String)RVMLIB_REC_MALLOC(len);
    RVMLIB_SET_RANGE(tgt, len);
    bcopy(src, tgt, len);

    return(tgt);
}


void Free_RPC2_String(RPC2_String& STR) {
    RVMLIB_REC_FREE(STR);
}


/*  *****  recov_daemon.c  *****  */

PRIVATE const int RecovDaemonInterval = 5;
PRIVATE	const int RecovDaemonStackSize = 262144;    /* MUST be big to handle rvm_trucates! */

PRIVATE char recovdaemon_sync;

void RECOVD_Init() {
    (void)new vproc("RecovDaemon", (PROCBODY)&RecovDaemon,
		     VPT_RecovDaemon, RecovDaemonStackSize);
}

void RecovDaemon() {
    /* Hack!!!  Vproc must yield before data members become valid! */
    /* suspect interaction between LWP creation/dispatch and C++ initialization. */
    VprocYield();

    vproc *vp = VprocSelf();
    RegisterDaemon(RecovDaemonInterval, &recovdaemon_sync);

    for (;;) {
        VprocWait(&recovdaemon_sync);

	/* First task is to get statistics. */
	Recov_GetStatistics();
	int WorkerIdleTime = GetWorkerIdleTime();

	/* Consider truncating. */
	unsigned long TruncateSize = RVM_OFFSET_TO_LENGTH(Recov_Statistics.log_written);
	if (TruncateSize >= MAXTS || WorkerIdleTime >= WITT)
	    RecovTruncate();

	/* Consider flushing. */
	Recov_TimeToFlush -= RecovDaemonInterval;
	unsigned long FlushSize = RVM_OFFSET_TO_LENGTH(Recov_Statistics.no_flush_length);
	if (Recov_TimeToFlush <= 0 || FlushSize >= MAXFS || WorkerIdleTime >= WITT)
	    RecovFlush();

	/* Bump sequence number. */
	vp->seq++;
    }
}
