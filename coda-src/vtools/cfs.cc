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

static char *rcsid = "$Header: /afs/cs.cmu.edu/user/clement/mysrcdir/coda-src/vtools/RCS/cfs.cc,v 1.2 1996/12/09 21:05:02 braam Exp $";
#endif /*_BLURB_*/






#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <libc.h>
#include <strings.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef LINUX
#include <sys/dirent.h>
#else
#include <sys/dir.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include <vice.h>
#include <venusioctl.h>
#include <prs_fs.h>


#ifdef LINUX
#define DIR dirent
#define direct dirent
#define d_namlen d_reclen
#endif

/* 

NOTE: This is a brand new cfs; it has been written from scratch
      and is NOT derived from fs in AFS-2 or AFS-3.  (Satya 3/20/92)

*/


#define NULL 0
#define PIOBUFSIZE 2048  /* max size of pioctl buffer */

char piobuf[PIOBUFSIZE];

typedef void (*PFV3)(int, char **, int);

/* Template of one cfs command */
struct command
    {
    char *opcode;	
    char *abbreviation;	/* NULL ==> no abbreviation */
    PFV3 handler;	/* Ptr to proc that can deal with this
			   The proc is invoked with 3 arguments:
				argc, argv and the index in cmdarray[]
				of this opcode (-1 if bogus opcode for help)*/
    char *usetxt;	/* Template specifying arguments to command */
    char *helptxt;	/* Text explaining what the command does */
    char *danger;	/* Text explaining dangerous consequences */
    };


/* One handler routine for each opcode */
PRIVATE void Bandwidth(int, char**, int);
PRIVATE void BeginRepair(int, char**, int);
PRIVATE void CheckServers(int, char**, int);
PRIVATE void CheckPointML(int, char**, int);
PRIVATE void CheckVolumes(int, char**, int);
PRIVATE void ClearPriorities(int, char**, int);
PRIVATE void Compress(int, char**, int);
PRIVATE void Disconnect(int, char**, int);
PRIVATE void DisableASR(int, char**, int);
PRIVATE void EnableASR(int, char**, int); 
PRIVATE void EndML(int, char**, int);
PRIVATE void EndRepair(int, char**, int);
PRIVATE void ExamineClosure(int, char**, int);
PRIVATE void FlushCache(int, char**, int);
PRIVATE void FlushObject(int, char**, int);
PRIVATE void FlushVolume(int, char**, int);
PRIVATE void FlushASR(int, char**, int); 
PRIVATE void GetFid(int, char**, int);
PRIVATE void GetPath(int, char**, int);
PRIVATE void GetMountPoint(int, char**, int);
PRIVATE void Help(int, char **, int);
PRIVATE void ListACL(int, char **, int);
PRIVATE void ListCache(int, char **, int);
PRIVATE void ListVolume(int, char **, int);
PRIVATE void LsMount(int, char**, int);
PRIVATE void MkMount(int, char**, int);
PRIVATE void PurgeML(int, char**, int);
PRIVATE void ReplayClosure(int, char**, int);
PRIVATE void Reconnect(int, char**, int);
PRIVATE void RmMount(int, char**, int);
PRIVATE void SetACL(int, char**, int);
PRIVATE void SetQuota(int, char **, int);
PRIVATE void SetVolume(int, char **, int);
PRIVATE void Slow(int, char **, int);
PRIVATE void TruncateLog(int, char **, int);
PRIVATE void Uncompress(int, char**, int);
PRIVATE void WaitForever(int, char**, int);
PRIVATE void WhereIs(int, char**, int);
PRIVATE void WriteDisconnect(int, char**, int);
PRIVATE void WriteReconnect(int, char**, int);
PRIVATE int IsObjInc(char *, ViceFid *);

/*  Array with one entry per command.
    To add new ones, just insert new 6-tuple, and add handler routine to list above.
    Note alphabetical order; the code doesn't rely on it, but it makes things easier to find.
    This array gets sequentially searched to parse and execute a command; it doesn't seem
	worthwhile being smarter (e.g. binary search or hash lookup)
*/

struct command cmdarray[] =
    {
	{"bandwidth", "bw", Bandwidth, 
	   "cfs bandwidth <speed>",
	   "Provide a hint about network speed",
	   NULL
     	},
	{"beginrepair", "br", BeginRepair, 
	   "cfs beginrepair <inc-obj-name>",
	   "Expose replicas of inc. objects",
	   NULL
     	},
	{"checkservers", NULL, CheckServers, 
	    "cfs checkservers <servernames>",
	    "Check up/down status of servers",
	    NULL
	},
	{"checkpointml", "ck", CheckPointML, 
	    "cfs checkpointml <dir> <checkpoint-dir>",
	    "Checkpoint volume modify log",
	    NULL
	},
	{"checkvolumes", NULL, CheckVolumes, 
	    "cfs checkvolumes",
	    "Check volume/name mappings",
	    NULL
	},
	{"clearpriorities", "cp", ClearPriorities, 
	    "cfs clearpriorities",
	    "Clear short-term priorities (DANGEROUS)",
	    "important files may be lost, if disconnected"
	},
	{"compress", NULL, Compress, 
	    "cfs compress <file> [<file> <file> ...]",
	    "Compress cached files",
	    NULL
	},
	{"disableasr", "dasr", DisableASR,
            "cfs disableasr <dir/file>",
            "Disable ASR execution in object's volume",
            NULL
	},
	{"disconnect", NULL, Disconnect, 
	    "cfs disconnect <servernames>",
	    "Partition from file servers (A LITTLE RISKY)",
	    NULL
	},
	{"enableasr", "easr", EnableASR,
            "cfs enableasr <dir/file>", 
            "Enable ASR execution in this volume",
            NULL
	},
	{"endrepair", "er", EndRepair,
	   "cfs endrepair <inc-obj-name>",
	   "Hide individual replicas of inc objects",
	   NULL
        },
	{"examineclosure", "ec", ExamineClosure, 
	    "cfs ec [-c] [<closure> <closure> ...]",
	    "Examine reintegration closure",
	    NULL
	},
	{"flushasr", "fasr", FlushASR,
            "cfs fasr <file-name>",
            "force the asr to get executed on next access",
            NULL
	},
	{"flushcache", NULL, FlushCache, 
	    "cfs flushcache",
	    "Flush entire cache (DANGEROUS)",
	    "all files will be lost, if disconnected"
	},
	{"flushobject", "fl", FlushObject, 
	    "cfs flushobject <obj>  [<obj> <obj> ...]",
	    "Flush objects from cache ",
	    NULL
	},
	{"flushvolume", NULL, FlushVolume, 
	    "cfs flushvolume  <dir> [<dir> <dir> ...]",
	    "Flush all data in volumes (DANGEROUS)",
	    "important files may be lost, if disconnected"
	},
	{"getfid", "gf", GetFid, 
	    "cfs getfid <path> [<path> <path> ...]",
	    "Map path to fid",
	    NULL
	},

	{"getpath", "gp", GetPath, 
	    "cfs getpath <fid> [<fid> <fid> ...]",
	    "Map fid to volume-relative path",
	    NULL
	},

	{"getmountpoint", "gmt", GetMountPoint, 
	    "cfs getmountpoint <volid> [<volid> <volid> ...]",
	    "Get mount point pathname for specified volume",
	    NULL
	},

	{"help", NULL, Help, 
	    "cfs help [opcode]",
	    "Type \"cfs help <opcode>\" for specific help on <opcode>",
	    NULL
	},
	{"listacl", "la", ListACL, 
	    "cfs listacl <dir> [<dir> <dir> ...]",
	    "List access control list ",
	    NULL
	},
	{"listcache", "lc", ListCache, 
	    "cfs listcache [-f <file>] [-l] [-ov] [-onv] [-all] [<vol> <vol> ...]",
	    "List cached fsobjs",
	    NULL
	},
	{"listvol", "lv", ListVolume, 
	    "cfs listvol <dir> [<dir> <dir> ...]",
	    "Display volume status",
	    NULL
	},
	{"lsmount", NULL, LsMount, 
	    "cfs lsmount <dir> [<dir> <dir> ...]",
	    "List mount point",
	    NULL
	},
	{"mkmount", "mkm", MkMount, 
	    "cfs mkmount <directory> <volume name>",
	    "Make mount point",
	    NULL
	},
	{"purgeml", NULL, PurgeML, 
	    "cfs purgeml <dir>",
	    "Purge volume modify log (DANGEROUS)",
	    "will destroy all changes made while disconnected"
	},
	{"reconnect", NULL, Reconnect, 
	    "cfs reconnect <servernames>",
	    "Heal partition to servers from cfs disconnect",
	    NULL
	},
	{"replayclosure", "rc", ReplayClosure, 
	    "cfs replayclosure [-i] [-r] [<closure> <closure> ...]",
	    "Replay reintegration closure",
	    NULL
	},
	{"rmmount", "rmm", RmMount, 
	    "cfs rmmount <dir> [<dir> <dir> ...]",
	    "Remove mount point ",
	    NULL
	},
	{"setacl", "sa", SetACL, 
	    "cfs setacl [-clear] [-negative] <dir> <id> <rights> [<id> <rights> ....]",
	    "Set access control list",
	    NULL
	},
        {"setquota", "sq", SetQuota,
	    "cfs setquota <dir> <blocks>",
	     "Set maximum disk quota",
	     NULL
	},
	{"setvol", "sv", SetVolume, 
	    "cfs setvol <dir> [-max <disk space quota in 1K units>] [-min <disk space guaranteed>] [-motd <message of the day>] [-offlinemsg <offline message>]",
	    "Set volume status ",
	    NULL
	},
	{"slow", NULL, Slow, 
	    "cfs slow <speed (bps)>",
	    "Set network speed",
	    NULL
	},
	{"truncatelog", "tl", TruncateLog, 
	    "cfs truncatelog",
	    "Truncate the RVM log at this instant",
	    NULL
	},
	{"uncompress", NULL, Uncompress, 
	    "cfs uncompress <file> [<file> <file> ...]",
	    "Uncompress cached files",
	    NULL
	},
	{"waitforever", "wf", WaitForever, 
	    "cfs waitforever [-on] [-off]",
	    "Control waitforever behavior",
	    NULL
	},
	{"whereis", NULL, WhereIs, 
	    "cfs whereis <dir> [<dir> <dir> ...]",
	    "List location of object",
	    NULL
	},
	{"writedisconnect", "wd", WriteDisconnect, 
	    "cfs writedisconnect [-age <sec>] [<dir> <dir> <dir> ...]",
	    "Write disconnect all volumes, or volumes specified",
	    NULL
	},
	{"writereconnect", "wr", WriteReconnect, 
	    "cfs writereconnect [<dir> <dir> <dir> ...]",
	    "Write connect all volumes, or volumes specified",
	    NULL
	}
    };

/* Number of commands in cmdarray */
int cmdcount = (int) (sizeof(cmdarray)/sizeof(struct command));

/* Access list definitions */
struct aclentry
    {
    char *id;
    int rights;
    };

struct acl
    {
    int pluscount;
    int minuscount;
    struct aclentry *plusentries; /* array of plus entries */
    struct aclentry *minusentries; /* array of plus entries */
    };

/* Type definitions for closure manipulations */
enum closure_ops {CLO_EXAMINE, CLO_REPLAY};
#define CLO_CONFLICTSONLY 0x1
#define CLO_INTERACTIVE	  0x2
#define CLO_REMOVE	  0x4

/* Type definitions for internal routines */
PRIVATE int findslot(char *s);
PRIVATE char *xlate_vvtype(ViceVolumeType vvt);
PRIVATE int parseacl(char *s, struct acl *a);
PRIVATE void translate(char *s, char oldc, char newc);
PRIVATE void fillrights(int x, char *s);
PRIVATE int getrights(char *s, int *x);
PRIVATE int getlongest(int argc, char *argv[]);
PRIVATE int dirincoda(char *);
PRIVATE int brave(int);
PRIVATE int doclosure(char *cloname, enum closure_ops opcode, int flags);
PRIVATE int findclosures(char ***clist);
PRIVATE int validateclosurespec(char *name, char *volname, char *volrootpath);


main(int argc, char *argv[])
    {
    int slot;
    
    /* First make stderr & stdout the same, to get perror() to print right */
    dup2(fileno(stdin), fileno(stderr));

    /* Next find and dispatch the opcode */
    if (argc >= 2)
	{
	slot = findslot(argv[1]);
	if (slot >=0)
	    {/* found it! */
	    if (cmdarray[slot].danger && !brave(slot)) exit(0);
	    ((PFV3)cmdarray[slot].handler)(argc, argv, slot); /* invoke the handler */
	    exit(0); /* and quit */
	    }
	}
    
    /* Opcode bogus or nonexistent */
    printf("Bogus or missing opcode: type \"cfs help\" for list\n");
    exit(-1);
    }

PRIVATE int brave(int slot)
    /* Warns user that an operation is dangerous and asks for confirmation.
       Returns TRUE if the user wants to go ahead, FALSE otherwise
    */
    {
    char response[10];

    printf("\tDANGER:   %s\n", cmdarray[slot].danger);
    printf("\tDo you really want to do this? [n] ");
    gets(response);

    if (strcmp(response, "y") == 0)
	{
	printf("\tFools rush in where angels fear to tread ........\n");
	return(1);
	}
    else
	{
	printf("\tDiscretion is the better part of valor!\n");
	return(0);
	}
    }


#define MAXHOSTS 8  /* from venus.private.h, should be in vice.h! */

PRIVATE void CheckServers(int argc, char *argv[], int opslot)
    {
    int rc, i; 
    unsigned long *downsrvarray;
    char *insrv=0;
    struct ViceIoctl vio;

    if (argc < 2 || argc > 10)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = PIOBUFSIZE;

    /* pack server host ids, if any */
    /* format of vio.in is #servers, hostid, hostid, ... */
    if (argc > 2)
	insrv = (char *) malloc(sizeof(int) + sizeof(unsigned long) * MAXHOSTS);

    int hcount = 0;
    for (i = 2; i < argc; i++) {
	struct hostent *h = gethostbyname(argv[i]);
	int ix = (int) (hcount * sizeof(unsigned long) + sizeof(int));
	if (h) {
	    *((unsigned long *) &insrv[ix]) = ntohl(*((unsigned long *)h->h_addr));
	    hcount++;
	}
    }	    
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(unsigned long) + sizeof(int));
    }

    printf("Contacting servers .....\n"); /* say something so Puneet knows something is going on */
    rc = pioctl("/coda", VIOCCKSERV, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOCCKSERV"); exit(-1);}

    /* See if there are any dead servers */
    if (insrv) free(insrv); /* free insrv only if it was alloc before */
    downsrvarray = (unsigned long *) piobuf;
    if (downsrvarray[0] == 0) {printf("All servers up\n"); return;}
    
    /* Print out names of dead servers */
    printf("These servers still down: ");
    for (i = 0; downsrvarray[i] != 0; i++)
	{
	long a;
	struct hostent *hent;

	a = htonl(downsrvarray[i]);
	if (a == 0) break;
	hent = gethostbyaddr((char *)&a, (int) sizeof(long), AF_INET);
	if (hent) printf("  %s", hent->h_name);
	else
	    {
	    a = downsrvarray[i]; /* a may have been clobbered by gethostbyaddr() */
	    printf("  %ld.%ld.%ld.%ld", (a&0xff000000) >> 24, (a&0x00ff0000) >> 16,
		    (a&0x0000ff00) >> 8, a&0x000000ff);
	    }
	}
    printf("\n");
    }


PRIVATE void CheckPointML(int argc, char* argv[], int opslot)
    {
    int rc; 
    struct ViceIoctl vio;
    char *ckpdir, *codadir;
    
    switch(argc)
	{
	case 2: codadir = "."; ckpdir = 0; break;
	
	case 3: codadir = argv[2]; ckpdir = 0; break;
	
	case 4: codadir = argv[2]; ckpdir = argv[3]; break;
	
	default: 
		printf("Usage: %s\n", cmdarray[opslot].usetxt);
		exit(-1);
	}

    /* Ensure that the checkpoint directory is NOT in Coda! */
    if (dirincoda(ckpdir))
	{
	printf("Checkpoint directory %s should NOT be in Coda\n", ckpdir);
	exit(-1);
	}
    
    /* Do the checkpoint */
    vio.in_size = (ckpdir ? (int) strlen(ckpdir) + 1 : 0);
    vio.in = ckpdir;
    vio.out_size = 0;
    vio.out = 0;
    rc = pioctl(codadir, VIOC_CHECKPOINTML, &vio, 1);
    if (rc < 0) {fflush(stdout); perror("VIOC_CHECKPOINTML"); exit(-1);}
    }


PRIVATE dirincoda(char *path)
    /*	Returns TRUE iff
	    (a) path is a directory
	    (b) you can cd to it right now (implying no intervening dangling sym links)
	    (c) the target is in Coda
	Returns FALSE in all other cases
    */
    {
    int rc;
    char buf[MAXPATHLEN];
    struct ViceIoctl vio;

    /* First see if you can chdir to the target */
    rc = chdir(path);
    if (rc < 0) return(0);
    
    /* Now find out where we really are */
    rc = (int)getwd(buf);
    if (rc == 0) return(0);
    
    /* Are we in Kansas, Toto? */
    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = PIOBUFSIZE;
    rc = pioctl(buf, VIOC_GETFID, &vio, 0);
    return(rc == 0);
    }

PRIVATE void CheckVolumes(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;

    if (argc != 2)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOCCKBACK, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_VIOCCKBACK"); exit(-1);}
    }

PRIVATE void ClearPriorities(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;

    if (argc != 2)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_CLEARPRIORITIES, &vio, 0);
    if (rc < 0){fflush(stdout); perror("  VIOC_CLEARPRIORITIES"); exit(-1);}
    }

PRIVATE void Compress(int argc, char *argv[], int opslot)
    {
    int i, rc; 
    struct ViceIoctl vio;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %s", argv[i]);
	vio.in = 0;
	vio.in_size = 0;
	vio.out = 0;
	vio.out_size = 0;
	rc = pioctl(argv[i], VIOC_COMPRESS, &vio, 1);
	if (rc < 0){fflush(stdout); perror("  VIOC_COMPRESS"); continue;}
	if (argc > 3) printf("\n");
	}
    }

PRIVATE void Disconnect(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;
    char *insrv = NULL;

    if (argc < 2 || argc > 10)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;

    /* pack server host ids, if any */
    /* format of vio.in is #servers, hostid, hostid, ... */
    if (argc > 2)
	insrv = (char *) malloc(sizeof(int) + sizeof(unsigned long) * MAXHOSTS);

    int hcount = 0;
    for (int i = 2; i < argc; i++) {
	struct hostent *h = gethostbyname(argv[i]);
	int ix = (int) (hcount * sizeof(unsigned long) + sizeof(int));
	if (h) {
	    *((unsigned long *) &insrv[ix]) = ntohl(*((unsigned long *)h->h_addr));
	    hcount++;
	}
    }	    
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(unsigned long) + sizeof(int));
    }

    rc = pioctl("/coda", VIOC_DISCONNECT, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_DISCONNECT"); exit(-1);}    

    if (insrv)
        free(insrv);
    }

PRIVATE void ExamineClosure(int argc, char *argv[], int opslot)
    {
    int rc, i, flags = 0;
    int first, last; /* indices of first & last closures in filenames[] */
    char **filenames;


    /* Obtain flags and  list of closures to be examined */
    for (i = 2; i < argc; i++)
	{
	if (argv[i][0] != '-') break;
	if (strcmp(argv[i], "-c") == 0) {flags |= CLO_CONFLICTSONLY; continue;}
	}

    if (i < argc)
	{
	first = i;
	last = argc - 1;
	filenames = argv; 
	}
    else 
	{/* No filenames specified on cmd line: look in /usr/coda/spool/<uid> */
	first = 0;
	last = findclosures(&filenames) - 1;
	if (last < 0) {printf("No closures found\n"); exit(-1);}
	}

    /* Then parse each closure */
    for (i = first; i <= last; i++) 
        {
	/* parse only .tar files */
	char *cp = strstr(filenames[i], ".tar");
	if (cp && cp != filenames[i])
	    {
	    rc = doclosure(filenames[i], CLO_EXAMINE, flags);
	    if (rc < 0) exit(-1);
	    }
	}
    }


PRIVATE int doclosure(char *cloname, enum closure_ops opcode, int flags)
    /* Code adapted from JJK's original implementations of {Examine,Replay}Closure
       Performs specified operation on cloname and prints results on stdout
       Returns 0 on success or non-fatal error, -1 on fatal error */
    {
    int rc = 0;
    char volname[32];
    char volrootpath[MAXPATHLEN];
    char cwd[MAXPATHLEN];
    char cmd[MAXPATHLEN];
    

    /* Sanity check args; ideally we should be checking combinations of (opcode,flags) */
    if (flags)
	{
	if (flags & CLO_CONFLICTSONLY)
	    {
	    printf("Conflict-only examination not yet implemented\n");
	    return(-1);
	    }

	if (flags & CLO_INTERACTIVE)
	    {
	    printf("Interactive replay not yet implemented\n");
	    return(-1);
	    }
	}

    /* Parse/Validate the closure specification. */
    if (!validateclosurespec(cloname, volname, volrootpath)) return(0);

    /* Remember the current working directory, then cd to the volume root. */
    if (!getwd(cwd)) {fflush(stdout); perror("getwd"); return(-1);}
    if (chdir(volrootpath) < 0){fflush(stdout); perror(volrootpath); return(0);}

    /* Construct and execute the appropriate command */
    switch (opcode)
	{
	case CLO_EXAMINE:
	    printf("\nExamining closure %s @ %s\n\n", volname, volrootpath); fflush(stdout);
	    sprintf(cmd, "tar tvf %s", cloname);
	    rc = system(cmd);
	    if (rc < 0) {fflush(stdout); printf("Couldn't read closure\n"); return(-1);}
	    break;
	    
	case CLO_REPLAY:
	    printf("\nReplaying closure %s @ %s\n\n", volname, volrootpath); fflush(stdout);
	    sprintf(cmd, "tar xvf %s", cloname);
	    rc = system(cmd);
	    if (rc < 0) {fflush(stdout); printf("Couldn't replay closure\n"); return(-1);}
	    if (flags & CLO_REMOVE)
		{
		rc = unlink(cloname);
		if (rc < 0) {fflush(stdout); perror(cloname); return(-1);}
		}
	    break;
	    
	default:
	    printf("Unknown opcode\n");
	    return(-1);
	}

    /* Return home */
    if (chdir(cwd) < 0) {fflush(stdout); perror(cwd); return(-1);}

    return(0);
    }


PRIVATE int findclosures(char ***clist)
    {/* Constructs an argv[]-like structure of closures in /usr/coda/spool/<uid>.
	Return the # of closures found.
     */
    int n = 0;
    char spooldir[MAXPATHLEN];
    DIR *dirp;
    struct direct *td;

    /*  XXXX  another hardwired path..... **  */

    sprintf(spooldir, "/usr/coda/spool/%d", getuid());
    dirp = opendir(spooldir);
    if (dirp == NULL){fflush(stdout); perror(spooldir); exit(-1);}

    while ((td = readdir(dirp)) != NULL)
	{
	if ((char *)index(td->d_name, '@') == NULL) continue;

	if (n == 0)
	    {
	    n = 1; 
	    *clist = (char **)malloc(sizeof(char *));
	    }
	else
	    {
	    n++;
	    *clist = (char **)realloc(*clist, n*sizeof(char *));
	    }
	(*clist)[n-1] = (char *)malloc(strlen(spooldir) + 1 + td->d_namlen + 1);
	strcpy((*clist)[n-1], spooldir);
	strcat((*clist)[n-1], "/");
	strcat((*clist)[n-1], td->d_name);
	}

    if (n == 0) printf("No closures found in %s\n", spooldir);
    return(n);
    }


PRIVATE int validateclosurespec(char *name, char *volname, char *volrootpath)
    {
    int rc;
    char *cp, *ap, *Volname;
    struct ViceIoctl vio;
    ViceFid *fid;

    /* Parse the closure spec into a <volname, volume-root path> pair. */

    cp = (char *)rindex(name, '/');
    if (cp == NULL) cp = name;
    else cp++;
    ap = (char *)index(cp, '@');
    if (ap == NULL)
	{
	printf("malformed closure spec (%s)\n", name);
	return(0);
	}
    strncpy(volname, cp, ap - cp);
    volname[ap - cp] = '\0';
    strcpy(volrootpath, ap + 1);

    /* remove .tar and .tar.old suffixes. */
    cp = strstr(volrootpath, ".tar");
    if (cp && cp != volrootpath) 
	*cp = '\0';

    /* Restore /'s in the volume-root path. */
    /* Of course, we're assuming that % is never legitimately used in a volume-root path! */
    for (cp = volrootpath; *cp; cp++)
	if (*cp == '%')
	    {
	    if (cp[1] == '%')
		{
		printf("malformed closure spec (%s)\n", name);
		return(0);
		}

	    *cp = '/';
	    }

    /* Verify that an object bound to the volume-root path exists (in Coda), */
    /* and that it is indeed the root of the volume with the given name. */

    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = PIOBUFSIZE;
    rc = pioctl(volrootpath, VIOCGETVOLSTAT, &vio, 1);
    if (rc) {fflush(stdout); perror(volrootpath); return(0);}

    Volname = piobuf + sizeof(VolumeStatus);
    if (strcmp(volname, Volname) != 0)
	{
	printf("%s is in wrong volume (%s, should be %s)\n",
	    volrootpath, Volname, volname);
	return(0);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = PIOBUFSIZE;
    rc = pioctl(volrootpath, VIOC_GETFID, &vio, 1);
    if (rc)  {fflush(stdout); perror(volrootpath); return(0);}
    fid = (ViceFid *)piobuf;
    if (fid->Vnode != 1 || fid->Unique != 1)
	{
	printf("%s is not the root of volume %s\n", volrootpath, volname);
	return(0);
	}

    return(1);
    }

PRIVATE void FlushCache(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;

    if (argc != 2)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_FLUSHCACHE, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_FLUSHCACHE"); exit(-1);}    
    }

PRIVATE void FlushObject(int argc, char *argv[], int opslot)
    {
    int i, w, rc;
    struct ViceIoctl vio;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}
    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */

	vio.in = 0;
	vio.in_size = 0;
	vio.out = 0;
	vio.out_size = 0;
	rc = pioctl(argv[i], VIOCFLUSH, &vio, 0);
	if (rc < 0)
	    {
	    fflush(stdout);
	    if (errno == EMFILE) printf("Can't flush active file\n");
	    else perror("  VIOCFLUSH");
	    continue;
	    }
	else {if (argc > 3) printf("\n");}
	}
    
    }


PRIVATE void FlushVolume(int argc, char *argv[], int opslot)
    {
    int i, w, rc;
    struct ViceIoctl vio;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}
    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */

	vio.in = 0;
	vio.in_size = 0;
	vio.out = 0;
	vio.out_size = 0;
	rc = pioctl(argv[i], VIOC_FLUSHVOLUME, &vio, 0);
	if (rc < 0) {fflush(stdout); perror("  VIOC_FLUSHVOLUME"); continue;}
	else {if (argc > 3) printf("\n");}
	}
    }

PRIVATE void BeginRepair(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;
    ViceFid fid;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    if (!IsObjInc(argv[2], &fid)) {
	printf("%s isn't inconsistent\n", argv[2]);
	exit(-1);
    }
    
    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = PIOBUFSIZE;
    vio.out = piobuf;
    bzero(piobuf, PIOBUFSIZE);

    rc = pioctl(argv[2], VIOC_ENABLEREPAIR, &vio, 0);
    if (rc < 0){fflush(stdout); perror("VIOC_ENABLEREPAIR"); exit(-1);}
}
PRIVATE void DisableASR(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = PIOBUFSIZE;
    vio.out = piobuf;
    bzero(piobuf, PIOBUFSIZE);

    rc = pioctl(argv[2], VIOC_DISABLEASR, &vio, 0);
    if (rc < 0){fflush(stdout); perror("VIOC_DISABLEASR"); exit(-1);}
}

PRIVATE void EnableASR(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = PIOBUFSIZE;
    vio.out = piobuf;
    bzero(piobuf, PIOBUFSIZE);

    rc = pioctl(argv[2], VIOC_ENABLEASR, &vio, 0);
    if (rc < 0){fflush(stdout); perror("VIOC_ENABLEASR"); exit(-1);}
}

PRIVATE void EndRepair(int argc, char *argv[], int opslot) 
{
    struct ViceIoctl vio;
    int rc;
    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = PIOBUFSIZE;
    vio.out = piobuf;
    bzero(piobuf, PIOBUFSIZE);

    rc = pioctl(argv[2], VIOC_DISABLEREPAIR, &vio, 0);
    if (rc < 0){fflush(stdout); perror("VIOC_DISABLEREPAIR"); exit(-1);}
}

PRIVATE void FlushASR(int argc, char *argv[], int opslot) {
    int i, rc, w;
    struct ViceIoctl vio;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)	{
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if >1 fid */
		/* Get its path */
	vio.in = 0;
	vio.in_size = 0;
	vio.out = piobuf;
	vio.out_size = PIOBUFSIZE;
	bzero(piobuf, PIOBUFSIZE);

	rc = pioctl(argv[i], VIOC_FLUSHASR, &vio, 0);
	if (rc < 0){fflush(stdout); perror("VIOC_FLUSHASR"); continue;}
	printf("\n"); 
    }
}


PRIVATE void GetFid(int argc, char *argv[], int opslot)
    {
    int i, rc, w;
    ViceFid fid;
    ViceVersionVector vv;
    struct ViceIoctl vio;
    char buf[100];

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */
	/* Validate next fid */

	/* Get its path */
	vio.in = 0;
	vio.in_size = 0;
	vio.out = piobuf;
	vio.out_size = PIOBUFSIZE;
	bzero(piobuf, PIOBUFSIZE);

	rc = pioctl(argv[i], VIOC_GETFID, &vio, 0);
	if (rc < 0){fflush(stdout); perror("VIOC_GETFID"); continue;}

	/* Got the fid!  Note that objects in conflict are trivial to find
	the fid for: just look at the dangling sym link value.  So we don't
	go to any trouble to find the fid if the object is in conflict.
	*/
	bcopy(piobuf, &fid, (int) sizeof(ViceFid));
	bcopy(piobuf+sizeof(ViceFid), &vv, (int) sizeof(ViceVersionVector));

	sprintf(buf, "0x%x.%x.%x",  fid.Volume, fid.Vnode, fid.Unique);
	printf("FID = %-20s     ", buf);
	sprintf(buf, "[%d %d %d %d %d %d %d %d]",
	       vv.Versions.Site0, vv.Versions.Site1,vv.Versions.Site2,
	       vv.Versions.Site3, vv.Versions.Site4,vv.Versions.Site5,
	       vv.Versions.Site6,vv.Versions.Site7);
	printf("VV = %-24s  ", buf);
	printf("STOREID = 0x%x.%x  FLAGS = 0x%x\n", vv.StoreId.Host, 
		vv.StoreId.Uniquifier, vv.Flags);
	}
    
    }

PRIVATE void GetPath(int argc, char *argv[], int opslot)
    {
    int i, rc, w;
    struct ViceIoctl vio;
    ViceFid fid;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */
	/* Validate next fid */
	if (sscanf(argv[i], "%lx.%lx.%lx", &fid.Volume, &fid.Vnode, &fid.Unique) != 3)
	    {
	    printf("Malformed fid: should look like %%x.%%x.%%x\n");
	    continue;
	    }

	/* Get its path */
	vio.in = (char *)&fid;
	vio.in_size = (int) sizeof(ViceFid);
	vio.out = piobuf;
	vio.out_size = PIOBUFSIZE;
	rc = pioctl("/coda", VIOC_GETPATH, &vio, 0);
	if (rc < 0){fflush(stdout); perror("VIOC_GETPATH"); continue;}
	printf("\t%s\n", vio.out);
	}
    
    }

PRIVATE void Help(int argc, char *argv[], int opslot)
    {
    int helpop; /* opcode for which help was requested */
    int i;
    
    if (argc < 3)
	{
	/* Generic help requested */
	printf("\n");
	for (i = 0; i < cmdcount; i++)
	    {
	    printf("%16s", cmdarray[i].opcode);
	    if (cmdarray[i].abbreviation) printf("  %3s  ", cmdarray[i].abbreviation);
	    else printf("       ");
	    printf("%s\n", cmdarray[i].helptxt);
	    }
	printf("\n");
	return;
	}
    
    /* Specific help requested */
    helpop = findslot(argv[2]);
    if (helpop < 0) printf("Bogus opcode: type \"cfs help\" for list\n");
    else printf("Usage: %s\n", cmdarray[helpop].usetxt);
    }


PRIVATE void ListACL(int argc, char *argv[], int opslot)
    {
    int i, j, rc;
    struct ViceIoctl vio;
    struct acl a;
    char rightsbuf[10];

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    for (i = 2; i < argc; i++)
	{
	vio.in = 0;
	vio.in_size = 0;
	vio.out_size = PIOBUFSIZE;
	vio.out = piobuf;
	rc = pioctl(argv[i], VIOCGETAL, &vio, 1);
	if (rc < 0) {perror(argv[i]); continue;}
	
	if (argc > 3) printf("\n%s:\n", argv[i]); /* show directory name */
	rc = parseacl(vio.out, &a);
	if (rc < 0)
	    {
	    printf("Venus returned an ill-formed ACL\n");
	    exit(-1);
	    }
	for (j = 0; j < a.pluscount; j++)
	    {
	    printf("%20s", a.plusentries[j].id); /* id */
	    fillrights(a.plusentries[j].rights, rightsbuf);
	    printf("  %-8s\n", rightsbuf); /* rights */ 
	    }
	for (j = 0; j < a.minuscount; j++)
	    {
	    printf("%20s", a.minusentries[j].id); /* id */
	    fillrights(a.minusentries[j].rights, rightsbuf);
	    printf(" -%-8s\n", rightsbuf); /* rights */ 
	    }
	}
    }

PRIVATE int parseacl(char *s, struct acl *a)
    /* 
	*s is assumed to be output of GETACL pioctl 
	Format:
		<plus count>\n
		<minus count>\n
		<plus id1><blanks><plus rights1>\n
		<plus id2><blanks><plus rights2>\n
		          ........
		<minus id1><blanks><minus rights1>\n
		<minus id2><blanks><minus rights2>\n
		          ........
	Note: *s gets clobbered by translate()
	
	Return 0 on success, -1 if unable to parse 
    */
    {
    int i;
    char *c, *r;

    translate(s, '\n', '\0');
    
    c = s;
    sscanf(c, "%d", &a->pluscount);
    c += strlen(c) + 1;
    if (a->pluscount > 0)
	{
	a->plusentries = (struct aclentry *) calloc(a->pluscount, sizeof(struct aclentry));
	assert(a->plusentries);
	}
    sscanf(c, "%d", &a->minuscount);
    c += strlen(c) + 1;
    if (a->minuscount > 0)
	{
	a->minusentries = (struct aclentry *) calloc(a->minuscount, sizeof(struct aclentry));
	assert(a->minusentries);
	}
    for (i = 0; i < a->pluscount; i++)
	{
	r = index(c, '\t');
	if (r == 0) return(-1);
	*r = 0;
	
	a->plusentries[i].id = (char *)malloc(strlen(c) + 1);
	strcpy(a->plusentries[i].id, c);
	c += strlen(c) + 1;
	if (*c == 0) return(-1);
	a->plusentries[i].rights = atoi(c);
	c += strlen(c) + 1;	
	}
    if (a->minuscount == 0) return(0);
    for (i = 0; i < a->minuscount; i++)
	{
	r = index(c, '\t');
	if (r == 0) return(-1);
	*r = 0;
	a->minusentries[i].id = (char *)malloc(strlen(c) + 1);
	strcpy(a->minusentries[i].id, c);
	c += strlen(c) + 1;
	if (*c == 0) return(-1);
	a->minusentries[i].rights = atoi(c);
	c += strlen(c) + 1;	
	}
    return(0);
    }


PRIVATE void fillrights(int x, char *s)
    /* Fills s with string corr to rights specified in x */
    {
    *s = 0;
    if (x & PRSFS_READ) strcat(s, "r");
    if (x & PRSFS_LOOKUP) strcat(s, "l");
    if (x & PRSFS_INSERT) strcat(s, "i");
    if (x & PRSFS_DELETE) strcat(s, "d");
    if (x & PRSFS_WRITE) strcat(s, "w");
    if (x & PRSFS_LOCK) strcat(s, "k");
    if (x & PRSFS_ADMINISTER) strcat(s, "a");
    }

PRIVATE int getrights(char *s, int *x)
    /* Sets x to rights specified in string s
       Returns 0 on success, -1 if s is bogus */
    {
    int i, len;

    if (strcmp(s, "none") == 0) s = "";
    if (strcmp(s, "all") == 0) s = "rlidwka";

    *x = 0;
    len = (int) strlen(s);

    for (i = 0; i < len; i++)
	{
	switch(s[i])
	    {
	    case 'r': *x |= PRSFS_READ; break;
	    case 'l': *x |= PRSFS_LOOKUP; break;
	    case 'i': *x |= PRSFS_INSERT; break;
	    case 'd': *x |= PRSFS_DELETE; break;
	    case 'w': *x |= PRSFS_WRITE; break;
	    case 'k': *x |= PRSFS_LOCK; break;
	    case 'a': *x |= PRSFS_ADMINISTER; break;
    
	    default: return(-1);
	    }
	}
    return(0);
    }


PRIVATE void translate(char *s, char oldc, char newc)
    /* Changes every occurence of oldc to newc in s */
    {
    int i, size;
    
    size = (int) strlen(s);
    for (i = 0; i < size; i++)
	if (s[i] == oldc) s[i] = newc;
    }

PRIVATE void GetMountPoint(int argc, char *argv[], int opslot)
{
  int i, rc;
  struct ViceIoctl vio;
  VolumeId vol_id;
  
  /* Parse command line arguments. */
  if (argc < 3) {
    printf("Usage: %s\n", cmdarray[opslot].usetxt);
    exit(-1);
  }

  for (i = 2; i < argc; i++) {
    if ( sscanf(argv[i], "%x", &vol_id) != 1 ) {
      printf("Usage: %s\n", cmdarray[opslot].usetxt);
      exit(-1);
    }
    vio.in = (char *)&vol_id;
    vio.in_size = (int) sizeof(VolumeId);
    vio.out = piobuf;
    vio.out_size = PIOBUFSIZE;

    /* Do the pioctl */
    rc = pioctl("/coda", VIOC_GET_MT_PT, &vio, 1);
    if (rc < 0) {
      fflush(stdout); perror("Failed in GetMountPoint."); exit(-1);
    }
    
    /* Print output field */
    printf("%x:  %s\n", vol_id, (char *)piobuf);
  }
}

PRIVATE void ListCache(int argc, char *argv[], int opslot)
{
  int i, rc;

  int long_format = 0;		/* If == 1, list in a long format. */
  unsigned int valid = 0;	/* list the following fsobjs, if
				   1: only valid, 2: only non-valid, 3: all */
  int file_specified = 0;	/* If == 0, list result to stdout. */
  int vol_specified = 0;	/* If == 0, all volumes cache status are listed. */
  char *filename = (char *)0;		/* Specified output file. */

  const int max_line = 256;
  char  *venus_file = "/tmp/_Venus_List_Cache";	/* Output file by Venus. */

  struct listcache_in {
    char fname[23];	/* strlen("/tmp/_Venus_List_Cache")+1 */
    int  first_volume;	/* if 1, Venus will unlink the file specified fname. */
    int  long_format;	/* = long_format */
    int  valid;		/* = valid */
  } data;

  struct ViceIoctl vio;
  char buf[max_line];

  /* Parse command line arguments. */
  if (argc < 2) {
    printf("Usage: %s\n", cmdarray[opslot].usetxt);
    exit(-1);
  }
  int argi;			/* Index for argv. */
  if (argc < 3) {
    file_specified = 0;
    vol_specified = 0;
    long_format = 0;
    valid = 3;		/* all fsobjs to be listed. */
  } else {
    for (argi = 2; (argi < argc) && (argv[argi][0] == '-'); argi++) {
      if (strcmp(argv[argi], "-f") == 0
	  || strcmp(argv[argi], "-file") == 0)
	if ( (argi + 1) < argc ) {
	  file_specified = 1;
	  filename = argv[++argi];
	} else {		/* filename is not specified as argv[argi+1]. */
	  printf("Usage: %s\n", cmdarray[opslot].usetxt);
	  exit(-1);
	}
      else if (strcmp(argv[argi], "-l") == 0)
	long_format = 1;
      else if (strcmp(argv[argi], "-ov") == 0)
	valid |= 1;
      else if (strcmp(argv[argi], "-onv") == 0)
	valid |= 2;
      else if (strcmp(argv[argi], "-a") == 0 ||
	       strcmp(argv[argi], "-all") == 0)
	valid |= 3;
      else {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
      }
    }
    if (argi == argc) vol_specified = 0;
    else vol_specified = 1;
    if (valid == 0) valid = 3;
  }

  /* for Debug */
  /*
     printf("file = %s, file_specified = %d, vol_specified = %d,\
            long? = %d, valid? = %d\n",
   	 filename, file_specified, vol_specified, long_format, valid);
   */

  if (vol_specified) {
    /* Volumes are specified. List cache for those volumes. */
    for (i = argi; i < argc; i++) {
      /* Set up parms to pioctl */
      VolumeId vol_id;
      char mtptpath[MAXPATHLEN];
      if ( sscanf(argv[i], "%x", &vol_id) == 1 ) {
	vio.in = (char *)&vol_id;
	vio.in_size = (int) sizeof(VolumeId);
	vio.out = piobuf;
	vio.out_size = PIOBUFSIZE;
	bzero(piobuf, PIOBUFSIZE);	

	/* Do the pioctl getting mount point pathname */
	rc = pioctl("/coda", VIOC_GET_MT_PT, &vio, 1);
	if (rc < 0) {
	  fflush(stdout); perror("Failed in GetMountPoint."); exit(-1);
	}
	strcpy(mtptpath, piobuf);
      }

      strcpy(data.fname, venus_file);
      data.first_volume = ((i == argi) ? 1 : 0);
      data.long_format = long_format;
      data.valid = valid;
      vio.in = (char *)&data;
      vio.in_size = (int) sizeof(struct listcache_in);
      vio.out_size = PIOBUFSIZE;
      vio.out = piobuf;
      bzero(piobuf, PIOBUFSIZE);

      /* Do the pioctl */
      if (vol_id)	/* VolumeId is specified. */
	rc = pioctl(mtptpath, VIOC_LISTCACHE_VOLUME, &vio, 1);
      else		/* Mount point pathname is specified. */
	rc = pioctl(argv[i], VIOC_LISTCACHE_VOLUME, &vio, 1);
      if (rc < 0) {fflush(stdout); perror(argv[i]); exit(-1);}
    }
  } else {
      /* No volumes are specified. List all cache volumes. */
      /* Set up parms to pioctl */
      strcpy(data.fname, venus_file);
      data.first_volume = 1;
      data.long_format = long_format;
      data.valid = valid;
      vio.in = (char *)&data;
      vio.in_size = (int) sizeof(struct listcache_in);

      vio.out_size = PIOBUFSIZE;
      vio.out = piobuf;
      bzero(piobuf, PIOBUFSIZE);
      /* Do the pioctl */
      rc = pioctl("/coda", VIOC_LISTCACHE, &vio, 1);
      if (rc < 0) {
	fflush(stdout); perror("Failed in ListCache."); exit(-1);
      }
    }

  /* List Cache Status.
   * cat the contens of venus_file to stdout or a specified file.
   */
  FILE *src_fp;
  FILE *dest_fp;
  if (file_specified)
    if ( (dest_fp = fopen(filename, "w") ) == NULL ) {
      printf("Cannot open file: %s\n", filename);
      exit(-1);
    }
  if ( src_fp = fopen(venus_file, "r") ) {
    while ( fgets(buf, max_line, src_fp) != NULL )
      if ( fputs(buf, (file_specified ? dest_fp : stdout) ) == EOF ) {
	printf("Output error\n");
	fclose(src_fp);
	if (file_specified) fclose(dest_fp);
	exit(-1);
      }
    fclose(src_fp);
    if (file_specified) fclose(dest_fp);
  }
}


PRIVATE void ListVolume(int argc, char *argv[], int opslot)
    {
    int i, rc;
    struct ViceIoctl vio;
    VolumeStatus *vs;
    char *volname, *omsg, *motd;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}


    for (i = 2; i < argc; i++)
	{
	/* Set up parms to pioctl */
	vio.in = 0;
	vio.in_size = 0;
	vio.out_size = PIOBUFSIZE;
	vio.out = piobuf;

	/* Do the pioctl */
	rc = pioctl(argv[i], VIOCGETVOLSTAT, &vio, 1);
	if (rc <0) {fflush(stdout); perror(argv[i]); continue;}
	
	/* Get pointers to output fields */
	vs = (VolumeStatus *)piobuf;
	volname = piobuf + sizeof(VolumeStatus);
	omsg = volname + strlen(volname) + 1;
	motd = omsg + strlen(omsg) + 1;

	/* Print output fields */
	if (argc > 3) printf("  %s:\n", argv[i]);  /* print directory name if more than one */
	printf("  Status of volume 0x%lx (%lu) named \"%s\"\n",
	    vs->Vid, vs->Vid, volname);
	if (*omsg) printf("  Offline message is \"%s\"\n", omsg);
	if (*motd) printf("  Message of the day is \"%s\"\n", motd);
	printf("  Volume type is %s\n", xlate_vvtype(vs->Type));
	printf("  Minimum quota is %lu,", vs->MinQuota);
	if (vs->MaxQuota > 0)
	    printf(" maximum quota is %lu\n", vs->MaxQuota);
	else printf(" maximum quota is unlimited\n");
	printf("  Current blocks used are %lu\n", vs->BlocksInUse);
	printf("  The partition has %lu blocks available out of %lu\n",
		vs->PartBlocksAvail, vs->PartMaxBlocks);
	printf("\n");
	}


    }


PRIVATE void LsMount (int argc, char *argv[], int opslot)
    /* This code will not detect a mount point where the root
       directory of the mounted volume denies permission for
       a chdir().  Hopefully this will be a rare event.
    */
    {
    int i, rc, w; 
    struct ViceIoctl vio;
    char part1[MAXPATHLEN], part2[MAXPATHLEN];
    char *s;
    struct stat stbuf;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{

	if (argc > 3) printf("  %*s  ", w, argv[i]);

	/* First see if it's a dangling mount point */
	rc = stat(argv[i], &stbuf);
	if (rc < 0 && errno == ENOENT)
	    {/* It's a nonexistent name, alright! */
	    rc = lstat(argv[i], &stbuf);
	    if (stbuf.st_mode & 0xff00 & S_IFLNK)
		{/* It is a sym link; read it */
		rc = readlink(argv[i], piobuf, (int) sizeof(piobuf));
		if (rc < 0) {fflush(stdout); perror("readlink"); continue;}
		/* Check if first char is a '#'; this is a heuristic, but rarely misleads */
		if (piobuf[0] == '#')
		    printf("Dangling sym link; looks like a mount point for volume \"%s\"\n", &piobuf[1]);
		else printf("Not a volume mount point\n", argv[i]);
		continue;
		}
	    }


	/* Next see if you can chdir to the target */
	rc = chdir(argv[i]);
	if (rc < 0)
	    {
	    if (errno == ENOTDIR) {printf("Not a mount point\n"); continue;}
	    else {fflush(stdout); perror("chdir"); continue;}
	    }

	/* We are in a directory, possibly the root of a volume */
	s = getwd(part1);
	if (s == 0) {fflush(stdout); perror("getwd"); continue;}
	s = rindex(part1, '/'); /* there must be a slash in what getwd() returns */
	*s = 0; /* lop off last component */
	strcpy(part2, s+1); /* and copy it */

	/* Ask Venus if this is a mount point */
	vio.in = part2;
	vio.in_size = (int) strlen(part2)+1;
	vio.out = piobuf;
	vio.out_size = PIOBUFSIZE;
	bzero(piobuf, PIOBUFSIZE);

	rc = pioctl(part1, VIOC_AFS_STAT_MT_PT, &vio, 0);
	if (rc < 0)
	    {
	    if (errno == EINVAL || errno == ENOTDIR) {printf("Not a mount point\n"); continue;}
	    else {fflush(stdout); perror("VIOC_AFS_STAT_MT_PT"); continue;}
	    }

	printf("Mount point for volume \"%s\"\n", &piobuf[1]);
	}
    }


PRIVATE void MkMount (int argc, char *argv[], int opslot)
    {
    int rc;
    char *vol, *dir;
    char buf[MAXPATHLEN+2];

    switch (argc)
	{
	case 4:   dir = argv[2]; vol = argv[3]; break;
	
	default: printf("Usage: %s\n", cmdarray[opslot].usetxt); exit(-1);
	}

    sprintf(buf, "#%s.", vol);
    rc = symlink(buf, dir);
    if (rc < 0) {fflush(stdout); perror(dir); exit(-1);}
    }

PRIVATE void PurgeML(int argc, char *argv[], int opslot)
    {
    int  rc;
    struct ViceIoctl vio;
    char *codadir;
    
    switch(argc)
	{
	case 3: codadir = argv[2]; break;
	
	default:
	    printf("Usage: %s\n", cmdarray[opslot].usetxt);
	    exit(-1);
	}


    vio.in_size = 0;
    vio.in = 0;
    vio.out_size = 0;
    vio.out = 0;
    rc = pioctl(codadir, VIOC_PURGEML, &vio, 1);
    if (rc) {fflush(stdout); perror("VIOC_PURGEML"); exit(-1);}
    }


PRIVATE void Reconnect(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;
    char *insrv = NULL;

    if (argc < 2 || argc > 10)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;

    /* pack server host ids, if any */
    /* format of vio.in is #servers, hostid, hostid, ... */
    if (argc > 2)
	insrv = (char *) malloc(sizeof(int) + sizeof(unsigned long) * MAXHOSTS);

    int hcount = 0;
    for (int i = 2; i < argc; i++) {
	struct hostent *h = gethostbyname(argv[i]);
	int ix = (int) (hcount * sizeof(unsigned long) + sizeof(int));
	if (h) {
	    *((unsigned long *) &insrv[ix]) = ntohl(*((unsigned long *)h->h_addr));
	    hcount++;
	}
    }	    
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(unsigned long) + sizeof(int));
    }

    rc = pioctl("/coda", VIOC_RECONNECT, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_RECONNECT"); exit(-1);}    

    if (insrv)
        free(insrv);
    }

PRIVATE void ReplayClosure(int argc, char *argv[], int opslot)
    {
    int rc, i, flags = 0;
    int first, last; /* indices of first & last closures in filenames[] */
    char **filenames;


    /* Obtain flags and  list of closures to be examined */
    for (i = 2; i < argc; i++)
	{
	if (argv[i][0] != '-') break;
	if (strcmp(argv[i], "-i") == 0) {flags |= CLO_INTERACTIVE; continue;}
	if (strcmp(argv[i], "-r") == 0) {flags |= CLO_REMOVE; continue;}
	}

    if (i < argc)
	{
	first = i;
	last = argc - 1;
	filenames = argv; 
	}
    else 
	{/* No filenames specified on cmd line: look in /usr/coda/spool/<uid> */
	first = 0;
	last = findclosures(&filenames) - 1;
	if (last < 0) {printf("No closures found\n"); exit(-1);}
	}

    /* Then replay each closure */
    for (i = first; i <= last; i++)
	{
	rc = doclosure(filenames[i], CLO_REPLAY, flags);
	if (rc < 0) exit(-1);
	}
    }

PRIVATE void RmMount(int argc, char *argv[], int opslot)
    {
    int  i, rc, w;
    struct ViceIoctl vio;
    char *prefix, *suffix;

    if (argc < 3) {printf("Usage: %s\n", cmdarray[opslot].usetxt);  exit(-1);}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{/* Remove one mount point per iteration */

	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one */

	/* First set up the prefix and suffix correctly */
	prefix = argv[i];
	suffix = rindex(prefix, '/');
	if (suffix)
	    {
	    *suffix = 0; /* shorten the prefix */
	    suffix++;  /* and skip the null */
	    }
	else
	    {
	    suffix = prefix;  
	    prefix = ".";
	    }
    
	/* Then do the pioctl */
	vio.in_size = (int) strlen(suffix) + 1;
	vio.in = suffix;
	vio.out_size = 0;
	vio.out = 0;
	rc = pioctl(prefix, VIOC_AFS_DELETE_MT_PT, &vio, 0);
	if (rc) {fflush(stdout); perror("VIOC_AFS_DELETE_MT_PT"); continue;}
	else {if (argc > 3) printf("\n");}
	}
    }

PRIVATE void SetACL (int argc, char *argv[], int opslot)
    {
    int i = 2, clearflag = 0, minusflag = 0, rc;
    char *dir;
    struct ViceIoctl vio;
    struct acl a;
    struct aclentry **aptr = &a.plusentries;
    int *asize = &a.pluscount;
    int nzcount; /* number of entries with non-zero rights in new acl */

    
    /* First parse "cfs sa [-clear] [-negative] <dir>" */
    if (i < argc && (strcmp(argv[i], "-clear") == 0)) {clearflag = 1; i++;}
    if (i < argc && (strcmp(argv[i], "-negative") == 0))
	{
	minusflag = 1;
	i++;
	aptr = &a.minusentries;
	asize = &a.minuscount;
	}
    if (clearflag && minusflag)
	{
	printf("Sorry, I won't let you specify -clear and -negative together\n");
	printf("Such an ACL would not allow access to anyone\n");
	exit(-1);
	}
    
    /* There must now be an odd number of items > 3 left to be parsed */
    if (((argc - i) < 3) || ((argc - i) % 2) != 1)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    /* Get the directory */
    dir = argv[i++];

    /* Get old acl */
    if (clearflag)
	{
	a.pluscount = a.minuscount = 0;
	a.plusentries = a.minusentries = NULL;
	}
    else 
	{
	vio.in = 0;
	vio.in_size = 0;
	vio.out_size = PIOBUFSIZE;
	vio.out = piobuf;
	rc = pioctl(dir, VIOCGETAL, &vio, 1);
	if (rc <0) {perror(dir); exit(-1);}
	rc = parseacl(vio.out, &a);
	if (rc < 0)
	    {
	    printf("Venus returned an ill-formed ACL\n");
	    exit(-1);
	    }
	}

    /* Walk remaining arguments and augment acl */
    while (i < argc)
	{/* Examine one pair per iteration */
	char *newid;
	int newrights, j;

	newid = argv[i];
	if (getrights(argv[i+1], &newrights) < 0)
	    {
	    printf("Bogus rights specified: \"%s\"\n", argv[i+1]);
	    exit(-1);
	    }

	/* If id already present, just replace rights */
	for(j = 0; j < *asize; j++)
	    {
	    if (CaseFoldedCmp((*aptr)[j].id, newid) == 0)
		{
		(*aptr)[j].rights = newrights;
		goto EntryDone;
		}
	    }
	/* No such id; create a new entry in ACL */
	(*asize)++;
	if (*asize == 1) *aptr = (struct aclentry *) malloc(sizeof(struct aclentry)); 
	else (*aptr) = (struct aclentry *) realloc(*aptr, (*asize)*sizeof(struct aclentry));
	(*aptr)[(*asize)-1].id = newid;
	(*aptr)[(*asize)-1].rights = newrights;

EntryDone:
	i += 2;  /* point to next (id,rights) pair */
	}
    
    /* Construct string encoding of ACL */

    /* First find out how many entries have non-zero rights */
    nzcount = 0;
    for (i = 0; i < a.pluscount; i++)
	{if (a.plusentries[i].rights != 0) nzcount++;}
    sprintf(piobuf, "%d\n", nzcount);

    nzcount = 0;
    for (i = 0; i < a.minuscount; i++)
	{if (a.minusentries[i].rights != 0) nzcount++;}
    sprintf(&piobuf[strlen(piobuf)], "%d\n", nzcount);

    /* Then walk the array and append entries with non-zero rights */
    for (i = 0; i < a.pluscount; i++)
	{
	if (a.plusentries[i].rights != 0)
	    sprintf(&piobuf[strlen(piobuf)], "%s %d\n", a.plusentries[i].id, a.plusentries[i].rights);
	}

    for (i = 0; i < a.minuscount; i++)
	{
	if (a.minusentries[i].rights != 0)
	    sprintf(&piobuf[strlen(piobuf)], "%s %d\n", a.minusentries[i].id, a.minusentries[i].rights);
	}

    /* Set new acl */
    vio.in = piobuf;
    vio.in_size = (int) strlen(piobuf) + 1;
    vio.out_size = 0;
    vio.out = 0;
    rc = pioctl(dir, VIOCSETAL, &vio, 1);
    if (rc <0) perror(dir);
    }


PRIVATE void SetVolume	(int argc, char *argv[], int opslot) {printf("Not supported by Coda yet\n");}


PRIVATE void SetQuota	(int argc, char *argv[], int opslot) 
{
    int i, rc;
    struct ViceIoctl vio;
    VolumeStatus *vs;

    if (argc < 4)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    /* There must be an even number of parameters */
    if (argc%2 == 1) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    for (i=2; i < argc; i+=2) {
        /* Set up parms to pioctl */
        vio.in = 0;
        vio.in_size = 0;
        vio.out_size = PIOBUFSIZE;
        vio.out = piobuf;

        /* Do the pioctl */
        rc = pioctl(argv[i], VIOCGETVOLSTAT, &vio, 1);
        if (rc <0) {fflush(stdout); perror(argv[i]); return;}
	
        /* Get pointers to output fields */
        vs = (VolumeStatus *)piobuf;
        printf("maximum quota is %lu\n", vs->MaxQuota);

        vs->MaxQuota = atoi(argv[i+1]);
        printf("New value of vs->MaxQuota will be set to %lu\n", vs->MaxQuota);

        vio.in_size  = PIOBUFSIZE;
        vio.in       = piobuf;
        vio.out_size = PIOBUFSIZE;
        vio.out      = piobuf;

        rc = pioctl(argv[i], VIOCSETVOLSTAT, &vio, 1);
        if (rc <0) {fflush(stdout); perror("Setting new quota"); return;}
    }
}


PRIVATE void Slow(int argc, char *argv[], int opslot) 
    {
    int rc;
    struct ViceIoctl vio;
    unsigned speed;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    speed = atoi(argv[2]);
    vio.in = (char *)&speed;
    vio.in_size = (int) sizeof(speed);
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_SLOW, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_SLOW"); exit(-1);}    
    }

PRIVATE void Bandwidth(int argc, char *argv[], int opslot) 
    {
    int rc;
    struct ViceIoctl vio;
    long speed;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    speed = atoi(argv[2]);
    vio.in = (char *)&speed;
    vio.in_size = (int) sizeof(speed);
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_BWHINT, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_BWHINT"); exit(-1);}    
    }

PRIVATE void TruncateLog(int argc, char *argv[], int opslot)
    {
    int rc;
    struct ViceIoctl vio;

    if (argc != 2)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_TRUNCATELOG, &vio, 1);
    if (rc < 0){fflush(stdout); perror("  VIOC_TRUNCATELOG"); exit(-1);}    
    }


PRIVATE void Uncompress(int argc, char *argv[], int opslot)
    {
    int i, rc; 
    struct ViceIoctl vio;

    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    for (i = 2; i < argc; i++)
	{
	if (argc > 3) printf("  %s", argv[i]);
	vio.in = 0;
	vio.in_size = 0;
	vio.out = 0;
	vio.out_size = 0;
	rc = pioctl(argv[i], VIOC_UNCOMPRESS, &vio, 1);
	if (rc < 0){fflush(stdout); perror("  VIOC_UNCOMPRESS"); continue;}
	if (argc > 3) printf("\n");
	}
    }

PRIVATE void WhereIs (int argc, char *argv[], int opslot)
    {
    int rc, i, j, w;
    struct ViceIoctl vio;
    long *custodians;
    struct hostent *hent;


    if (argc < 3)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
	{

	/* Set up parms to pioctl */
	vio.in = 0;
	vio.in_size = 0;
	vio.out_size = PIOBUFSIZE;
	vio.out = piobuf;

	/* Do the pioctl */
	rc = pioctl(argv[i], VIOCWHEREIS, &vio, 1);
	if (rc <0) {fflush(stdout); perror(argv[i]); continue;}
	
	/* Print custodians */
	if (argc > 3) printf("  %*s:  ", w, argv[i]);
	custodians = (long *)piobuf; /* pioctl returns array of IP addrs */
	for (j = 0; j < 8; j++)
	    {
	    long a;
	    
	    a = htonl(custodians[j]);
	    if (a == 0) break;
	    hent = gethostbyaddr((char *)&a, (int) sizeof(long), AF_INET);
	    if (hent) printf("  %s", hent->h_name);
	    else
		{
		a = custodians[j]; /* a may have been clobbered by gethostbyaddr() */
		printf("  %ld.%ld.%ld.%ld", (a&0xff000000) >> 24, (a&0x00ff0000) >> 16,
			(a&0x0000ff00) >> 8, a&0x000000ff);
		}
	    }
	printf("\n");
	}
    }


PRIVATE void WaitForever (int argc, char *argv[], int opslot)
    {
    int rc, value = -1;
    struct ViceIoctl vio;

    if (argc == 3)
	{
	if (strcmp(argv[2], "-on") == 0) value = 1;
	if (strcmp(argv[2], "-off") == 0) value = 0;
	}

    if (value == -1)
	{
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
	}

    vio.in = (caddr_t)&value;
    vio.in_size = (short) sizeof(value);
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl("/coda", VIOC_WAITFOREVER, &vio, 0);
    if (rc < 0){fflush(stdout); perror("  VIOC_WAITFOREVER"); exit(-1);}
    }


PRIVATE void WriteDisconnect(int argc, char *argv[], int opslot)
    {
    int  i = 2, rc, w = 0;
    struct ViceIoctl vio;
    unsigned age = (unsigned)-1, time = (unsigned)-1;  /* huge*/
    
    if (i < argc && (strcmp(argv[i], "-age") == 0)) {age=atoi(argv[i+1]); i+=2;}
    if (i < argc && (strcmp(argv[i], "-time") == 0)) {time=atoi(argv[i+1]); i+=2;}

    ((unsigned *)piobuf)[0] = age;
    ((unsigned *)piobuf)[1] = time;
    vio.in = piobuf;
    vio.in_size = 2 * (int) sizeof(unsigned);
    vio.out = 0;
    vio.out_size = 0;

    if (argc == i) 	/* no more args -- do them all */
        {
	rc = pioctl("/coda", VIOC_WD_ALL, &vio, 1);	
	if (rc) {fflush(stdout); perror("VIOC_WD_ALL"); exit(-1);}
	}
    else 
        {
	w = getlongest(argc, argv);
	for (int j = i; j < argc; j++) 
	    {
	    if (argc > i+1) printf("  %*s\n", w, argv[j]); /* echo input if more than one fid */

	    rc = pioctl(argv[j], VIOC_BEGINML, &vio, 0);
	    if (rc < 0) {fflush(stdout); perror("  VIOC_BEGINML"); exit(-1);}
	    }
        }
    }


PRIVATE void WriteReconnect(int argc, char *argv[], int opslot)
    {
    int  i, w, rc;
    struct ViceIoctl vio;
    
    vio.in_size = 0;
    vio.in = 0;
    vio.out_size = 0;
    vio.out = 0;

    if (argc == 2) 	/* do them all */
        {
	rc = pioctl("/coda", VIOC_WR_ALL, &vio, 1);	
	if (rc) {fflush(stdout); perror("VIOC_WR_ALL"); exit(-1);}
        }    
    else 
        {
	w = getlongest(argc, argv);
	for (i = 2; i < argc; i++) 
	    {
	    if (argc > 3) printf("  %*s\n", w, argv[i]); /* echo input if more than one fid */

	    rc = pioctl(argv[i], VIOC_ENDML, &vio, 1);
	    if (rc) {fflush(stdout); perror("VIOC_ENDML"); exit(-1);}
	    }
	}
    }


PRIVATE int findslot(char *s)
    /* Returns the index in cmdarray[] of opcode or abbreviation s;
       returns -1 if no such opcode */
    {
    int i;

    for (i = 0; i < cmdcount; i++)
	{
	if ((strcmp(s, cmdarray[i].opcode) == 0) || 
	    (cmdarray[i].abbreviation && strcmp(s, cmdarray[i].abbreviation) == 0))
	return(i);
	}
    return(-1);
    }

PRIVATE char *xlate_vvtype(ViceVolumeType vvt)
    {
    switch(vvt)
	{
	case ReadOnly: return("ReadOnly");
	case ReadWrite: return("ReadWrite");
	case Backup: return("Backup");
	case Replicated: return("Replicated");
	default: return("????");
	}
    }


PRIVATE int getlongest(int argc, char *argv[])
    {/* Return length of longest argument; for use in aligning printf() output */
    int i, max, next;

    max = 0;
    for (i = 2; i < argc; i++)
	{
	next = (int) strlen(argv[i]);
	if (max < next) max = next;
	}
    return(max);
    }


PRIVATE int IsObjInc(char *name, ViceFid *fid) {
    int rc;
    char symval[MAXPATHLEN];
    struct stat statbuf;

    fid->Vnode = 0; fid->Unique = 0; fid->Volume = 0;

    rc = stat(name, &statbuf);
    if ((rc == 0) || (rc == ENOENT)) return(0);

    /* is it a sym link */
    symval[0] = 0;
    rc = readlink(name, symval, MAXPATHLEN);
    if (rc < 0) return(0);
    
    /* it's a sym link, alright  */
    if (symval[0] == '@') 
	sscanf(symval, "@%x.%x.%x",
	       &fid->Volume, &fid->Vnode, &fid->Unique);
    return(1);
}


/* *** CODE STATUS ****

Not supported in Venus yet:
---------------------------
          setvol
            slow
     truncatelog


Not tested:
-----------
    checkpointml (can't test until Brian fixes printf() death in Venus)
   replayclosure
  examineclosure

Tested but dubious:
-------------------
 clearpriorities (can't tell whether this is really doing anything)
     waitforever (cfs is ok, but Venus behavior is strange)


Why does gethostbyaddr() take such long timeouts when disconnected?
*/
