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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/librepair/repio.h,v 4.1 1997/01/08 21:50:09 rvb Exp $";
#endif /*_BLURB_*/








/* Created:
	M. Satyanarayanan
	June 1989

*/

/*  Directory repair data structures and routines 	

    A directory repair involves a list of individual repair operations.
    This list is constructed on the client and executed on the server.
    (In automated resolution, gets constructed on server)
    The repair is executed within a transaction at the server.
    struct repair defines one step in the repair.
    It should really be a union type but this is painful in C.
*/
   
#define REPAIR_MAX 5  /* Max no of integer parms for a repair entry */
#define MAXNAMELEN 255 /* Max len of pathname component: should this be defined 
                          somewhere in /usr/include/sys? */

struct repair
    {
    unsigned opcode;   		/* values defined below */
    char name[MAXNAMELEN];  	/* overloaded; null-terminated */
    char newname[MAXNAMELEN];	/* used only by rename */
    unsigned parms[REPAIR_MAX];  	/* overloaded */
    };

/* Repair opcodes */

#define REPAIR_OPBASE 174320 	/* random */

/* Creation: 
    repair.name  specifies object name
    repair.parms[0..2] specify FID to be associated with this name
*/
#define REPAIR_CREATEF 	REPAIR_OPBASE+1 	/* Create file */
#define REPAIR_CREATED	REPAIR_OPBASE+2		/* Create directory */
#define REPAIR_CREATES	REPAIR_OPBASE+3		/* Create sym link */
#define REPAIR_CREATEL	REPAIR_OPBASE+4		/* Create (hard) link */
				
/* Remove opcodes:
    repair.name specifies object name
    repair.parms ignored
*/
#define REPAIR_REMOVEFSL	REPAIR_OPBASE+5 /* Remove file or (hard) link */
#define REPAIR_REMOVED		REPAIR_OPBASE+6 /* Remove dir */

/* ACL mods: 
    repair.name gives name of user in acl
    repair.parms[0] specifies rights
*/
/* REPAIR_DELACL is removed; repair.parms[0] = 0 means delete entry */
#define REPAIR_SETACL	REPAIR_OPBASE+7		/* Set rights */
#define REPAIR_SETNACL	REPAIR_OPBASE+8		/* Set negative rights */


/* Status mods:
    repair.name is ignored
    repair.parms[0] gives new value
*/
#define REPAIR_SETMODE		REPAIR_OPBASE+9
#define REPAIR_SETOWNER		REPAIR_OPBASE+10  /* Have to be a sys administrator for this */
#define REPAIR_SETMTIME		REPAIR_OPBASE+11

/* Next replica:
    repair.name is the name of the server
       repair.parms[0] is a hex number, giving read-write volid of this replica
    
    Seen only in ASCII input files typed by users
*/
#define REPAIR_REPLICA		REPAIR_OPBASE+12

#define REPAIR_RENAME		REPAIR_OPBASE+13	/* mv object */



/* The structure below is the header for the repairs to be made to one replica.
   An array of such structures can describe the repairs to all replicas.
   The number of replicas cannot be static, since disconnected operation can
   produce one conflicting replica per client.
*/
   
struct listhdr
    {
    VolumeId replicaId;		/* unique id of this replica (== its rw volid) */
    unsigned int repairCount;	/* number of entries in array repairList[] */
    struct repair *repairList;	/* pointer to array of repair entries for this replica */
    };

/* Function specification */

#ifndef C_ARGS
#if  (__cplusplus | __STDC__)
#define C_ARGS(arglist) arglist
#else   __cplusplus
#define C_ARGS(arglist) ()
#endif  __cplusplus
#endif  C_ARGS

/* I/O routines */
int repair_putdfile C_ARGS((char *fname, int replicaCount, struct listhdr *replicaList));
int repair_getdfile C_ARGS((char *fname, int *replicaCount, struct listhdr **replicaList));
int repair_parseline C_ARGS((char *line, struct repair *rs));
int repair_parsefile C_ARGS((char *fname, int *hdcount, struct listhdr **hdarrray));
void repair_printline C_ARGS((struct repair *rs, FILE *ff));
void repair_printfile C_ARGS((char *fname));
