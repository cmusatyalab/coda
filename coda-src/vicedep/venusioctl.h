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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vicedep/venusioctl.h,v 4.2 1997/02/26 16:03:47 rvb Exp $";
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


/*
 *
 *  Definitions of Venus-specific ioctls
 *
 */


#ifndef _VENUS_IOCTL_H_
#define _VENUS_IOCTL_H_ 1

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef __MACH__
#include <sys/viceioctl.h>
#endif /* __MACH__ */
#if defined(__linux__) || defined(__BSD44__)
#include <cfs/mach_vioctl.h> /* new identity of sys/viceioctl.h */
#endif /* __BSD44__ */

#ifdef __cplusplus
}
#endif __cplusplus

/* Fix some brain-damage in the just included file. */
#undef	_VICEIOCTL
#ifdef	__STDC__
#define _VICEIOCTL(id)  (/*(unsigned int ) */_IOW('V', id, struct ViceIoctl))
#else
#define _VICEIOCTL(id)  (/*(unsigned int ) */_IOW(V, id, struct ViceIoctl))
#endif
#undef	_VALIDVICEIOCTL
#define _VALIDVICEIOCTL(com) (com >= _VICEIOCTL(0) && com <= _VICEIOCTL(255))

/* Definitions of Venus-specific ioctls  */

/* IOCTLS to Venus.  Apply these to open file decriptors. */
#define	VIOCCLOSEWAIT		_VICEIOCTL(1)	/* Force close to wait for store */
#define	VIOCABORT		_VICEIOCTL(2)	/* Abort close on this fd */
#define	VIOCIGETCELL		_VICEIOCTL(3)	/* ioctl to get cell name */

/* PIOCTLS to Venus.  Apply these to path names with pioctl. */
#define	VIOCSETAL		_VICEIOCTL(1)	/* Get access control list */
#define	VIOCGETAL		_VICEIOCTL(2)	/* Set access control list */
#define	VIOCSETTOK		_VICEIOCTL(3)	/* Set authentication tokens */
#define	VIOCGETVOLSTAT		_VICEIOCTL(4)	/* Get volume status */
#define	VIOCSETVOLSTAT		_VICEIOCTL(5)	/* Set volume status */
#define	VIOCFLUSH		_VICEIOCTL(6)	/* Invalidate cache entry */
#define	VIOCSTAT		_VICEIOCTL(7)	/* Get file status */
#define	VIOCGETTOK		_VICEIOCTL(8)	/* Get authentication tokens */
#define	VIOCUNLOG		_VICEIOCTL(9)	/* Invalidate tokens */
#define	VIOCCKSERV		_VICEIOCTL(10)	/* Check that servers are up */
#define	VIOCCKBACK		_VICEIOCTL(11)	/* Check backup volume mappings */
#define	VIOCCKCONN		_VICEIOCTL(12)	/* Check connections for a user */
#define	VIOCGETTIME		_VICEIOCTL(13)	/* Do a vice gettime for performance testing */
#define	VIOCWHEREIS		_VICEIOCTL(14)	/* Find out where a volume is located */
#define	VIOCPREFETCH		_VICEIOCTL(15)	/* Prefetch a file */
#define	VIOCNOP			_VICEIOCTL(16)	/* Do nothing (more preformance) */
#define	VIOCENGROUP		_VICEIOCTL(17)	/* Enable group access for a group */
#define	VIOCDISGROUP		_VICEIOCTL(18)	/* Disable group access */
#define	VIOCLISTGROUPS		_VICEIOCTL(19)	/* List enabled and disabled groups */
#define	VIOCACCESS		_VICEIOCTL(20)	/* Access using PRS_FS bits */
#define	VIOCUNPAG		_VICEIOCTL(21)	/* Invalidate pag */
#define	VIOCGETWD		_VICEIOCTL(22)	/* Get wdir quickly */
#define	VIOCWAITFOREVER		_VICEIOCTL(23)	/* Wait for dead servers forever */
#define	VIOCSETCACHESIZE	_VICEIOCTL(24)	/* Set venus cache size in 1k units */
#define	VIOCFLUSHCB		_VICEIOCTL(25)	/* Flush callback only */
#define	VIOCNEWCELL		_VICEIOCTL(26)	/* Configure new cell */
#define VIOCGETCELL		_VICEIOCTL(27)	/* Get cell info */
#define	VIOC_AFS_DELETE_MT_PT	_VICEIOCTL(28)	/* [AFS] Delete mount point */
#define VIOC_AFS_STAT_MT_PT	_VICEIOCTL(29)	/* [AFS] Stat mount point */
#define	VIOC_FILE_CELL_NAME	_VICEIOCTL(30)	/* Get cell in which file lives */
#define	VIOC_GET_WS_CELL	_VICEIOCTL(31)	/* Get cell in which workstation lives */
#define VIOC_AFS_MARINER_HOST	_VICEIOCTL(32)	/* [AFS] Get/set mariner host */
#define VIOC_GET_PRIMARY_CELL	_VICEIOCTL(33)	/* Get primary cell for caller */
#define	VIOC_VENUSLOG		_VICEIOCTL(34)	/* Enable/Disable venus logging */
#define	VIOC_GETCELLSTATUS	_VICEIOCTL(35)	/* get cell status info */
#define	VIOC_SETCELLSTATUS	_VICEIOCTL(36)	/* set corresponding info */
#define	VIOC_FLUSHVOLUME	_VICEIOCTL(37)	/* flush whole volume's data */
#define	VIOC_LISTCACHE_VOLUME	_VICEIOCTL(39)	/* list volume's cached status */

#define	CFS_IOCTL_BASE	192

#define	VIOC_ENABLEREPAIR	_VICEIOCTL(CFS_IOCTL_BASE + 0)	/* Enable repair for vol/pag combo */
#define	VIOC_DISABLEREPAIR	_VICEIOCTL(CFS_IOCTL_BASE + 1)	/* Disable repair for vol/pag combo */
#define	VIOC_REPAIR		_VICEIOCTL(CFS_IOCTL_BASE + 2)	/* Repair an object */

#define	VIOC_GETSERVERSTATS	_VICEIOCTL(CFS_IOCTL_BASE + 3)	/* Get host statistics */
#define	VIOC_GETVENUSSTATS	_VICEIOCTL(CFS_IOCTL_BASE + 4)	/* Get Venus statistics */
#define	VIOC_GETFID		_VICEIOCTL(CFS_IOCTL_BASE + 5)	/* Get ViceFid */

#define	VIOC_FLUSHCACHE		_VICEIOCTL(CFS_IOCTL_BASE + 6)	/* Flush entire FSO cache */

#define	VIOC_SETVV		_VICEIOCTL(CFS_IOCTL_BASE + 7)	/* Overwrite a version vector */

#define	VIOC_HDB_ADD		_VICEIOCTL(CFS_IOCTL_BASE + 8)	/* Add hoard entry. */
#define	VIOC_HDB_DELETE		_VICEIOCTL(CFS_IOCTL_BASE + 9)	/* Delete hoard entry. */
#define VIOC_HDB_MODIFY		_VICEIOCTL(CFS_IOCTL_BASE + 10)	/* Modify hoard entry. */
#define VIOC_HDB_CLEAR		_VICEIOCTL(CFS_IOCTL_BASE + 11)	/* Clear hoard database. */
#define VIOC_HDB_LIST		_VICEIOCTL(CFS_IOCTL_BASE + 12)	/* List hoard database. */

#define	VIOC_WAITFOREVER	_VICEIOCTL(CFS_IOCTL_BASE + 13)	/* Control waitforever behavior. */

#define	VIOC_HDB_WALK		_VICEIOCTL(CFS_IOCTL_BASE + 14)	/* Walk hoard database. */
#define	VIOC_CLEARPRIORITIES	_VICEIOCTL(CFS_IOCTL_BASE + 15)	/* Clear short-term priorities. */

#define	VIOC_GETPATH		_VICEIOCTL(CFS_IOCTL_BASE + 16)	/* Map Fid to vol-relative path. */

#define	VIOC_COMPRESS		_VICEIOCTL(CFS_IOCTL_BASE + 17)	/* Compress an object. */
#define	VIOC_UNCOMPRESS		_VICEIOCTL(CFS_IOCTL_BASE + 18)	/* Uncompress an object. */

#define	VIOC_CHECKPOINTML	_VICEIOCTL(CFS_IOCTL_BASE + 19)	/* Checkpoint a volume's ML. */
#define	VIOC_PURGEML		_VICEIOCTL(CFS_IOCTL_BASE + 20)	/* Purge a volume's ML. */

#define	VIOC_BEGINRECORDING	_VICEIOCTL(CFS_IOCTL_BASE + 21)	/* Begin recording references. */
#define	VIOC_ENDRECORDING	_VICEIOCTL(CFS_IOCTL_BASE + 22)	/* End recording references. */
#define VIOC_TRUNCATELOG        _VICEIOCTL(CFS_IOCTL_BASE + 23) /* Truncate the log */
#define VIOC_DISCONNECT         _VICEIOCTL(CFS_IOCTL_BASE + 24) /* Partition from all servers */
#define VIOC_RECONNECT          _VICEIOCTL(CFS_IOCTL_BASE + 25) /* Heal previous partition */
#define VIOC_SLOW               _VICEIOCTL(CFS_IOCTL_BASE + 26) /* Slow the network down */
#define VIOC_GETPFID            _VICEIOCTL(CFS_IOCTL_BASE + 27) /* Get fid of object's parent */
#define VIOC_BEGINML            _VICEIOCTL(CFS_IOCTL_BASE + 28) /* Log updates to a volume */
#define VIOC_ENDML              _VICEIOCTL(CFS_IOCTL_BASE + 29) /* Stop logging (write back) */

#define	VIOC_HDB_VERIFY		_VICEIOCTL(CFS_IOCTL_BASE + 30)	/* Compare hoard database to cache contents */
#define	VIOC_BWHINT		_VICEIOCTL(CFS_IOCTL_BASE + 31)	/* Give a bandwidth hint */

#define VIOC_HDB_ENABLE		_VICEIOCTL(CFS_IOCTL_BASE + 32)	/* Enable hoard walks */
#define VIOC_HDB_DISABLE	_VICEIOCTL(CFS_IOCTL_BASE + 33)	/* Disable hoard walks */

#define VIOC_ENABLEASR		_VICEIOCTL(CFS_IOCTL_BASE + 34)	/* Enable ASRs for a volume */
#define VIOC_DISABLEASR		_VICEIOCTL(CFS_IOCTL_BASE + 35)	/* Disable ASRs for a volume */
#define VIOC_FLUSHASR		_VICEIOCTL(CFS_IOCTL_BASE + 36) /* Flush timestamp of last ASR invocation */ 

#define VIOC_REP_BEGIN          _VICEIOCTL(CFS_IOCTL_BASE + 37)	/* Begin a local/global repair session */
#define VIOC_REP_END            _VICEIOCTL(CFS_IOCTL_BASE + 38) /* End a local/global repair session */
#define VIOC_REP_CHECK          _VICEIOCTL(CFS_IOCTL_BASE + 39) /* check the current local mutation */
#define VIOC_REP_PRESERVE       _VICEIOCTL(CFS_IOCTL_BASE + 40) /* Preserve current local mutation */
#define VIOC_REP_DISCARD        _VICEIOCTL(CFS_IOCTL_BASE + 41) /* Discard current local mutation */
#define VIOC_REP_REMOVE    	_VICEIOCTL(CFS_IOCTL_BASE + 42) /* remove rest of the local mutations */
#define VIOC_SET_LOCAL_VIEW     _VICEIOCTL(CFS_IOCTL_BASE + 43) /* Set local repair session view */
#define VIOC_SET_GLOBAL_VIEW    _VICEIOCTL(CFS_IOCTL_BASE + 44) /* Set global repair session view */
#define VIOC_SET_MIXED_VIEW     _VICEIOCTL(CFS_IOCTL_BASE + 45) /* Set mixed repair session view */
#define VIOC_WD_ALL 		_VICEIOCTL(CFS_IOCTL_BASE + 46) /* Write-disconnect all volumes */
#define VIOC_WR_ALL     	_VICEIOCTL(CFS_IOCTL_BASE + 47) /* Write-reconnect all volumes */
#define	VIOC_LISTCACHE		_VICEIOCTL(CFS_IOCTL_BASE + 50)	/* List cache status */
#define	VIOC_GET_MT_PT		_VICEIOCTL(CFS_IOCTL_BASE + 51)	/* Get mount point path from volume id */

/* 
 * The following change is made in order to reduce the number of command codes for the 
 * local/global repair interface. We now use only one pioctl command code for the interface
 * the actual repair command are passed as an additional parameter, as defined as follows.
 * This new command VIOC_REP_CMD and its associated constants are intended to replace the 
 * previous 9 commands such as VIOC_REP_BEGIN etc. that are used for local/global repair.
 * We let the old command to exist for now in order to reduce the occasions of inconvenience
 * of the imcompatibility between venus and tools such as cfs and repair. We will later perform
 * one major pioctl interface cleanup later, and that will require every user to pick up the 
 * right venus and the right tools.
 */
#define VIOC_REP_CMD  	        _VICEIOCTL(CFS_IOCTL_BASE + 52)	/* new local-global repair command */
#define	REP_CMD_BEGIN		1
#define REP_CMD_END		2
#define	REP_CMD_CHECK		3
#define REP_CMD_PRESERVE	4
#define REP_CMD_PRESERVE_ALL	5
#define REP_CMD_DISCARD		6
#define REP_CMD_DISCARD_ALL	7
#define REP_CMD_LIST		8
#define REP_CMD_LOCAL_VIEW	9
#define REP_CMD_GLOBAL_VIEW	10
#define REP_CMD_MIXED_VIEW	11

#endif	not _VENUS_IOCTL_H_
