/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

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


/*
 *
 *  Definitions of Venus-specific ioctls
 *
 */


#ifndef _VENUS_IOCTL_H_
#define _VENUS_IOCTL_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <pioctl.h> /* new identity of sys/viceioctl.h */

#ifdef __cplusplus
}
#endif

#define CFS_PIOBUFSIZE 2048 /* max size of in,out data in pioctls (Satya, 1/03) */


/* Definitions of Venus-specific ioctls  */

/* IOCTLS to Venus.  Apply these to open file decriptors. */
/* these seem to collide with the pioctl ones. */
#define _VIOCCLOSEWAIT		(1)	/* Force close to wait for store */
#define _VIOCABORT		(2)	/* Abort close on this fd */
#define _VIOCIGETCELL		(3)	/* ioctl to get cell name */

/* PIOCTLS to Venus.  Apply these to path names with pioctl. */
/* These need to be wrapped in _VICEIOCTL() to make a usable value */
#define _VIOCSETAL		(1)	/* Get access control list */
#define _VIOCGETAL		(2)	/* Set access control list */
#define _VIOCSETTOK		(3)	/* Set authentication tokens */
#define _VIOCGETVOLSTAT		(4)	/* Get volume status */
#define _VIOCSETVOLSTAT		(5)	/* Set volume status */
#define _VIOCFLUSH		(6)	/* Invalidate cache entry */
#define _VIOCSTAT		(7)	/* Get file status */
#define _VIOCGETTOK		(8)	/* Get authentication tokens */
#define _VIOCUNLOG		(9)	/* Invalidate tokens */
#define _VIOCCKSERV		(10)	/* Check that servers are up */
#define _VIOCCKBACK		(11)	/* Check backup volume mappings */
#define _VIOCCKCONN		(12)	/* Check connections for a user */
#define _VIOCWHEREIS		(14)	/* Find out where a volume is located */
#define _VIOCPREFETCH		(15)	/* Prefetch a file */
#define _VIOCNOP		(16)	/* Do nothing (more preformance) */
#define _VIOCENGROUP		(17)	/* Enable group access for a group */
#define _VIOCDISGROUP		(18)	/* Disable group access */
#define _VIOCLISTGROUPS		(19)	/* List enabled and disabled groups */
#define _VIOCACCESS		(20)	/* Access using PRS_FS bits */
#define _VIOCUNPAG		(21)	/* Invalidate pag */
#define _VIOCGETWD		(22)	/* Get wdir quickly */
#define _VIOCWAITFOREVER	(23)	/* Wait for dead servers forever */
#define _VIOCSETCACHESIZE	(24)	/* Set venus cache size in 1k units */
#define _VIOCFLUSHCB		(25)	/* Flush callback only */
#define _VIOCNEWCELL		(26)	/* Configure new cell */
#define _VIOCGETCELL		(27)	/* Get cell info */
#define _VIOC_AFS_DELETE_MT_PT	(28)	/* [AFS] Delete mount point */
#define _VIOC_AFS_STAT_MT_PT	(29)	/* [AFS] Stat mount point */
#define _VIOC_FILE_CELL_NAME	(30)	/* Get cell in which file lives */
#define _VIOC_GET_WS_CELL	(31)	/* Get cell in which workstation lives */
#define _VIOC_AFS_MARINER_HOST	(32)	/* [AFS] Get/set mariner host */
#define _VIOC_GET_PRIMARY_CELL	(33)	/* Get primary cell for caller */
#define _VIOC_VENUSLOG		(34)	/* Enable/Disable venus logging */
#define _VIOC_GETCELLSTATUS	(35)	/* get cell status info */
#define _VIOC_SETCELLSTATUS	(36)	/* set corresponding info */
#define _VIOC_FLUSHVOLUME	(37)	/* flush whole volume's data */
#define _VIOC_LISTCACHE_VOLUME	(39)	/* list volume's cached status */
#define _VIOC_LOOKASIDE         (40) /* Add or remove  cache lookaside databases (Satya, 1/2003) */

/* These were defined with numbers that wrapped around the 8-bit size of the nr
 * component in the ioctl */
#define _VIOC_BEGINWB            (136) /* start writeback from volume id */
#define _VIOC_ENDWB              (137) /* end writeback from volume id */
#define _VIOC_STATUSWB           (138) /* fetch status from volume id */
#define _VIOC_AUTOWB             (139) /* toggle autowriteback */
#define _VIOC_SYNCCACHE          (140) /* reintegrate now ! */
#define _VIOC_REDIR		 (141) /* redirect to a staging server */
#define _VIOC_ADD_MT_PT		 (142) /* Add mount point */


#define CFS_IOCTL_BASE	192

#define _VIOC_ENABLEREPAIR	(CFS_IOCTL_BASE + 0)	/* Enable repair for vol/pag combo */
#define _VIOC_DISABLEREPAIR	(CFS_IOCTL_BASE + 1)	/* Disable repair for vol/pag combo */
#define _VIOC_REPAIR		(CFS_IOCTL_BASE + 2)	/* Repair an object */

#define _VIOC_GETSERVERSTATS	(CFS_IOCTL_BASE + 3)	/* Get host statistics */
#define _VIOC_GETVENUSSTATS	(CFS_IOCTL_BASE + 4)	/* Get Venus statistics */
#define _VIOC_GETFID		(CFS_IOCTL_BASE + 5)	/* Get ViceFid */

#define _VIOC_FLUSHCACHE	(CFS_IOCTL_BASE + 6)	/* Flush entire FSO cache */

#define _VIOC_SETVV		(CFS_IOCTL_BASE + 7)	/* Overwrite a version vector */

#define _VIOC_HDB_ADD		(CFS_IOCTL_BASE + 8)	/* Add hoard entry. */
#define _VIOC_HDB_DELETE	(CFS_IOCTL_BASE + 9)	/* Delete hoard entry. */
#define _VIOC_HDB_MODIFY	(CFS_IOCTL_BASE + 10)	/* Modify hoard entry. */
#define _VIOC_HDB_CLEAR		(CFS_IOCTL_BASE + 11)	/* Clear hoard database. */
#define _VIOC_HDB_LIST		(CFS_IOCTL_BASE + 12)	/* List hoard database. */

#define _VIOC_WAITFOREVER	(CFS_IOCTL_BASE + 13)	/* Control waitforever behavior. */

#define _VIOC_HDB_WALK		(CFS_IOCTL_BASE + 14)	/* Walk hoard database. */
#define _VIOC_CLEARPRIORITIES	(CFS_IOCTL_BASE + 15)	/* Clear short-term priorities. */

#define _VIOC_GETPATH		(CFS_IOCTL_BASE + 16)	/* Map Fid to vol-relative path. */

#define _VIOC_COMPRESS		(CFS_IOCTL_BASE + 17)	/* Compress an object. */
#define _VIOC_UNCOMPRESS	(CFS_IOCTL_BASE + 18)	/* Uncompress an object. */

#define _VIOC_CHECKPOINTML	(CFS_IOCTL_BASE + 19)	/* Checkpoint a volume's ML. */
#define _VIOC_PURGEML		(CFS_IOCTL_BASE + 20)	/* Purge a volume's ML. */

#define _VIOC_BEGINRECORDING	(CFS_IOCTL_BASE + 21)	/* Begin recording references. */
#define _VIOC_ENDRECORDING	(CFS_IOCTL_BASE + 22)	/* End recording references. */
#define _VIOC_TRUNCATELOG       (CFS_IOCTL_BASE + 23) /* Truncate the log */
#define _VIOC_DISCONNECT        (CFS_IOCTL_BASE + 24) /* Partition from all servers */
#define _VIOC_RECONNECT         (CFS_IOCTL_BASE + 25) /* Heal previous partition */
#define _VIOC_SLOW              (CFS_IOCTL_BASE + 26) /* Slow the network down */
#define _VIOC_GETPFID           (CFS_IOCTL_BASE + 27) /* Get fid of object's parent */
#define _VIOC_BEGINML           (CFS_IOCTL_BASE + 28) /* Log updates to a volume */
#define _VIOC_ENDML             (CFS_IOCTL_BASE + 29) /* Stop logging (write back) */

#define _VIOC_HDB_VERIFY	(CFS_IOCTL_BASE + 30)	/* Compare hoard database to cache contents */
/* #define	_VIOC_BWHINT	(CFS_IOCTL_BASE + 31)	* Give a bandwidth hint */

#define _VIOC_HDB_ENABLE	(CFS_IOCTL_BASE + 32)	/* Enable hoard walks */
#define _VIOC_HDB_DISABLE	(CFS_IOCTL_BASE + 33)	/* Disable hoard walks */

#define _VIOC_ENABLEASR		(CFS_IOCTL_BASE + 34)	/* Enable ASRs for a volume */
#define _VIOC_DISABLEASR	(CFS_IOCTL_BASE + 35)	/* Disable ASRs for a volume */
#define _VIOC_FLUSHASR		(CFS_IOCTL_BASE + 36) /* Flush timestamp of last ASR invocation */ 

#define _VIOC_REP_BEGIN         (CFS_IOCTL_BASE + 37)	/* Begin a local/global repair session */
#define _VIOC_REP_END           (CFS_IOCTL_BASE + 38) /* End a local/global repair session */
#define _VIOC_REP_CHECK         (CFS_IOCTL_BASE + 39) /* check the current local mutation */
#define _VIOC_REP_PRESERVE      (CFS_IOCTL_BASE + 40) /* Preserve current local mutation */
#define _VIOC_REP_DISCARD       (CFS_IOCTL_BASE + 41) /* Discard current local mutation */
#define _VIOC_REP_REMOVE    	(CFS_IOCTL_BASE + 42) /* remove rest of the local mutations */
#define _VIOC_SET_LOCAL_VIEW    (CFS_IOCTL_BASE + 43) /* Set local repair session view */
#define _VIOC_SET_GLOBAL_VIEW   (CFS_IOCTL_BASE + 44) /* Set global repair session view */
#define _VIOC_SET_MIXED_VIEW    (CFS_IOCTL_BASE + 45) /* Set mixed repair session view */
#define _VIOC_WD_ALL 		(CFS_IOCTL_BASE + 46) /* Write-disconnect all volumes */
#define _VIOC_WR_ALL     	(CFS_IOCTL_BASE + 47) /* Write-reconnect all volumes */
#define _VIOC_STRONG		(CFS_IOCTL_BASE + 48)	/* Force strong connectivity */
#define _VIOC_ADAPTIVE		(CFS_IOCTL_BASE + 49)	/* Allow bandwidth adaptation driven connectivity */
#define _VIOC_LISTCACHE		(CFS_IOCTL_BASE + 50)	/* List cache status */
#define _VIOC_GET_MT_PT		(CFS_IOCTL_BASE + 51)	/* Get mount point path from volume id */

/*  The following change is made in order to reduce the number of
 * command codes for the local/global repair interface. We now use
 * only one pioctl command code for the interface the actual repair
 * command are passed as an additional parameter, as defined as
 * follows.  This new command VIOC_REP_CMD and its associated
 * constants are intended to replace the previous 9 commands such as
 * VIOC_REP_BEGIN etc. that are used for local/global repair.  We let
 * the old command to exist for now in order to reduce the occasions
 * of inconvenience of the imcompatibility between venus and tools
 * such as cfs and repair. We will later perform one major pioctl
 * interface cleanup later, and that will require every user to pick
 * up the right venus and the right tools.  */
#define _VIOC_REP_CMD  	        (CFS_IOCTL_BASE + 52)	/* new local-global repair command */
#define REP_CMD_BEGIN		1
#define REP_CMD_END		2
#define REP_CMD_CHECK		3
#define REP_CMD_PRESERVE	4
#define REP_CMD_PRESERVE_ALL	5
#define REP_CMD_DISCARD		6
#define REP_CMD_DISCARD_ALL	7
#define REP_CMD_LIST		8
#define REP_CMD_LOCAL_VIEW	9
#define REP_CMD_GLOBAL_VIEW	10
#define REP_CMD_MIXED_VIEW	11

#define _VIOC_UNLOADKERNEL       (CFS_IOCTL_BASE + 53) /* Unload kernel module, only Win9x so far */

/* we really can't/shouldn't go beyond 255 (CFS_IOCTL_BASE + 63) because the nr
 * component in an ioctl is only an 8-bit value.
 * The following ioctls probably ended up either clobbering the ioctl number,
 * or they wrapped around and collided with some low numbers. */
#if 0
#define _VIOC_BEGINWB	(CFS_IOCTL_BASE + 200) /* start writeback from volume id */
#define _VIOC_ENDWB	(CFS_IOCTL_BASE + 201) /* end writeback from volume id */
#define _VIOC_STATUSWB	(CFS_IOCTL_BASE + 202) /* fetch status from volume id */
#define _VIOC_AUTOWB	(CFS_IOCTL_BASE + 203) /* toggle autowriteback */
#define _VIOC_SYNCCACHE	(CFS_IOCTL_BASE + 204) /* reintegrate now ! */
#define _VIOC_REDIR	(CFS_IOCTL_BASE + 205) /* redirect to a staging server */
#define _VIOC_ADD_MT_PT	(CFS_IOCTL_BASE + 206) /* Add mount point */
#endif

#endif /* _VENUS_IOCTL_H_ */

