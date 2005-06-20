/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"
#include "coda_assert.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#ifdef  __linux__
#if defined(__GLIBC__) && __GLIBC__ >= 2
#include <dirent.h>
#else
#include <sys/dirent.h>
#endif
#endif

#include <inodeops.h>

#ifdef __cplusplus
}
#endif


#include <util.h>
#include <vice.h>
#include <venusioctl.h>
#include <prs.h>
#include <codaconf.h>
#include <inconsist.h>
#include <coda_config.h>

/* From venusvol.h.  A volume is in exactly one of these states. */
typedef enum {	Hoarding,
		Emulating,
		Logging,
		Resolving,
} VolumeStateType;

#ifdef  __linux__
#define direct dirent
#define d_namlen d_reclen
#endif

/* 

NOTE: This is a brand new cfs; it has been written from scratch
      and is NOT derived from fs in AFS-2 or AFS-3.  (Satya 3/20/92)

*/

#define PERROR(desc) do { fflush(stdout); perror(desc); } while(0)

char piobuf[CFS_PIOBUFSIZE];

typedef void (*PFV3)(int, char **, int);

/* Template of one cfs command */
struct command
    {
    char *opcode;       
    char *abbreviation; /* NULL ==> no abbreviation */
    PFV3 handler;       /* Ptr to proc that can deal with this
                           The proc is invoked with 3 arguments:
                                argc, argv and the index in cmdarray[]
                                of this opcode (-1 if bogus opcode for help)*/
    char *usetxt;       /* Template specifying arguments to command */
    char *helptxt;      /* Text explaining what the command does */
    char *danger;       /* Text explaining dangerous consequences */
    };


/* One handler routine for each opcode */
static void Adaptive(int, char**, int);
static void BeginRepair(int, char**, int);
static void CheckServers(int, char**, int);
static void CheckPointML(int, char**, int);
static void CheckVolumes(int, char**, int);
static void ClearPriorities(int, char**, int);
static void Disconnect(int, char**, int);
static void DisableASR(int, char**, int);
static void EnableASR(int, char**, int); 
static void EndRepair(int, char**, int);
static void ExamineClosure(int, char**, int);
static void FlushCache(int, char**, int);
static void FlushObject(int, char**, int);
static void FlushVolume(int, char**, int);
static void FlushASR(int, char**, int); 
static void GetFid(int, char**, int);
static void GetPFid(int, char**, int);
static void MarkFidIncon(int, char**, int);
static void ExpandObject(int, char**, int);
static void CollapseObject(int, char**, int);
static void GetPath(int, char**, int);
static void GetMountPoint(int, char**, int);
static void Help(int, char **, int);
static void ListACL(int, char **, int);
static void ListCache(int, char **, int);
static void ListVolume(int, char **, int);
static void LookAside(int, char **, int);
static void LsMount(int, char**, int);
static void MkMount(int, char**, int);
static void PurgeML(int, char**, int);
static void Redir(int, char**, int);
static void ReplayClosure(int, char**, int);
static void Reconnect(int, char**, int);
static void RmMount(int, char**, int);
static void SetACL(int, char**, int);
static void SetQuota(int, char **, int);
static void SetVolume(int, char **, int);
static void Strong(int, char**, int);
static void TruncateLog(int, char **, int);
static void UnloadKernel(int, char **, int);
static void WaitForever(int, char**, int);
static void WhereIs(int, char**, int);
static void ForceReintegrate(int, char**, int);
static void WriteDisconnect(int, char**, int);
static void WriteReconnect(int, char**, int);

static void At_SYS(int, char **, int);
static void At_CPU(int, char **, int);

/*  Array with one entry per command.
    To add new ones, just insert new 6-tuple, and add handler routine to
    list above. Note alphabetical order; the code doesn't rely on it,
    but it makes things easier to find. This array gets sequentially
    searched to parse and execute a command; it doesn't seem worthwhile
    being smarter (e.g. binary search or hash lookup)
*/

struct command cmdarray[] =
    {
        {"adaptive", NULL, Adaptive, 
           "cfs adaptive",
           "allow venus to automatically adapt to bandwidth changes",
           NULL
        },
        {"strong", NULL, Strong, 
           "cfs strong",
           "force venus to consider all connections strong",
           NULL
        },
        {"beginrepair", "br", BeginRepair, 
           "cfs beginrepair <inc-obj-name>",
           "Expose replicas of inc. objects",
           NULL
        },
        {"checkservers", "cs", CheckServers, 
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
        {"disableasr", "dasr", DisableASR,
            "cfs disableasr <dir/file>",
            "Disable ASR execution in object's volume",
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
        {"cpuname", "@cpu", At_CPU, 
            "cfs {cpuname|@cpu}",
            "print the @cpu expansion for the current platform",
            NULL
        },
        {"sysname", "@sys", At_SYS, 
            "cfs {sysname|@sys}",
            "print the @sys expansion for the current platform",
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
            "these files will be lost, if disconnected"
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
        {"getpfid", NULL, GetPFid, 
            "cfs getpfid <fid> [<fid> <fid> ...]",
            "Map fid to parent fid",
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
            "cfs listvol [-local] <dir> [<dir> <dir> ...]",
            "Display volume status (-local avoids querying server)",
            NULL
        },
        {"lookaside", "lka", LookAside, 
	 "cfs lookaside [--clear] +/-<db1> +/-<db2> +/-<db3> ....\n       cfs lookaside --list\n",
            "Add, remove or list cache lookaside databases",
            NULL
        },
        {"lsmount", NULL, LsMount, 
            "cfs lsmount <dir> [<dir> <dir> ...]",
            "List mount point",
            NULL
        },
        {"markincon", NULL, MarkFidIncon, 
            "cfs markincon <path> [<path> <path> ...]",
            "Mark object in conflict",
            "this meddles with the version vector and can trash the object"
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
            "cfs setacl [-clear] [-negative] <dir> <name> <rights> [<name> <rights> ....]",
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
        {"truncatelog", "tl", TruncateLog, 
            "cfs truncatelog",
            "Truncate the RVM log at this instant",
            NULL
        },
        {"unloadkernel", "uk", UnloadKernel,
            "cfs unloadkernel",
            "Unloads the kernel module",
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
        {"redir", NULL, Redir, 
            "cfs redir <dir> <ip-address>",
            "Redirect volume to a staging server",
            NULL
        },
        {"disconnect", NULL, Disconnect, 
            "cfs disconnect <servernames>",
            "Partition from file servers (A LITTLE RISKY)",
            NULL
        },
        {"reconnect", NULL, Reconnect, 
            "cfs reconnect <servernames>",
            "Heal partition to servers from cfs disconnect",
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
        },
	{"forcereintegrate", "fr", ForceReintegrate,
	    "cfs forcereintegrate <dir> [<dir> <dir> ...]",
	    "Force modifications in a disconnected volume to the server",
	    NULL
	},
        {"expand", NULL, ExpandObject,
            "cfs expand <path> [<path> <path> ...]",
            "Expand object into a fake directory, exposing the underlying versions",
            NULL
        },
        {"collapse", NULL, CollapseObject,
            "cfs collapse <path> [<path> <path> ...]",
            "Collapse expanded object (see cfs expand)",
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
#define CLO_INTERACTIVE   0x2
#define CLO_REMOVE        0x4

/* Type definitions for internal routines */
static int findslot(char *s);
static char *xlate_vvtype(ViceVolumeType vvt);
static char *print_conn_state(VolumeStateType);
static int parseacl(char *s, struct acl *a);
static void translate(char *s, char oldc, char newc);
static void fillrights(int x, char *s);
static int getrights(char *s, int *x);
static int getlongest(int argc, char *argv[]);
static int dirincoda(char *);
static int brave(int);
static int doclosure(char *cloname, enum closure_ops opcode, int flags);
static int findclosures(char ***clist);
static int validateclosurespec(char *name, char *volname, char *volrootpath);


int main(int argc, char *argv[])
{
    int slot;

    if (argc < 2) goto fail;

    /* Find and dispatch the opcode */
    slot = findslot(argv[1]);
    if (slot < 0) goto fail;
    
    /* found it! */
    if (cmdarray[slot].danger) {
    	if (!brave(slot))
	    exit(0);
    }

    /* invoke the handler */
    ((PFV3)cmdarray[slot].handler)(argc, argv, slot);

    exit(0);
fail:
    /* Opcode bogus or nonexistent */
    printf("Bogus or missing opcode: type \"cfs help\" for list\n");
    exit(-1);
}
    

static int brave(int slot)
    /* Warns user that an operation is dangerous and asks for confirmation.
       Returns TRUE if the user wants to go ahead, FALSE otherwise
    */
    {
    char response[10];

    printf("\tDANGER:   %s\n", cmdarray[slot].danger);
    printf("\tDo you really want to do this? [n] ");
    fgets(response, 10, stdin);

    if (response[0] == 'y')
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

static int parseHost(char *name_or_ip, struct in_addr *addr)
{
    struct hostent *h;

    if (inet_aton(name_or_ip, addr))
	return 1;

    h = gethostbyname(name_or_ip);
    if (!h) {
	addr->s_addr = INADDR_ANY;
	return 0;
    }

    *addr = *(struct in_addr *)h->h_addr;
    return 1;
}


#define MAXHOSTS 8  /* from venus.private.h, should be in vice.h! */

static void CheckServers(int argc, char *argv[], int opslot)
{
    int rc, i; 
    struct in_addr *downsrvarray;
    char *insrv=0;
    struct ViceIoctl vio;

    if (argc < 2 || argc > 10) {
        printf("Usage: %s\n", cmdarray[opslot].usetxt);
        exit(-1);
    }

    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = CFS_PIOBUFSIZE;

    /* pack server host ids, if any */
    /* format of vio.in is #servers, hostid, hostid, ... */
    if (argc > 2)
        insrv = (char *) malloc(sizeof(int) + sizeof(struct in_addr) * MAXHOSTS);

    int hcount = 0;
    for (i = 2; i < argc; i++) {
        int ix = (int) (hcount * sizeof(struct in_addr) + sizeof(int));
	struct in_addr host;

	if (!parseHost(argv[i], &host)) continue;

	*((struct in_addr *) &insrv[ix]) = host;
	hcount++;
    }
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(struct in_addr) + sizeof(int));
    }

    printf("Contacting servers .....\n"); /* say something so Puneet knows something is going on */
    rc = pioctl(NULL, _VICEIOCTL(_VIOCCKSERV), &vio, 1);
    if (rc < 0) { PERROR("VIOCCKSERV"); exit(-1); }

    /* See if there are any dead servers */
    if (insrv) free(insrv); /* free insrv only if it was alloc before */
    downsrvarray = (struct in_addr *) piobuf;
    if (downsrvarray[0].s_addr == 0) {printf("All servers up\n"); return;}

    /* Print out names of dead servers */
    printf("These servers still down: ");
    for (i = 0; downsrvarray[i].s_addr != 0; i++) {
	struct hostent *hent;

	hent = gethostbyaddr((char *)&downsrvarray[i], sizeof(long), AF_INET);
	if (hent) printf("  %s", hent->h_name);
	else      printf("  %s", inet_ntoa(downsrvarray[i]));
    }
    printf("\n");
}


static void CheckPointML(int argc, char* argv[], int opslot)
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
    rc = pioctl(codadir, _VICEIOCTL(_VIOC_CHECKPOINTML), &vio, 1);
    if (rc < 0) { PERROR("VIOC_CHECKPOINTML"); exit(-1); }
}


static int dirincoda(char *path)
/*  Returns TRUE iff
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
    if (getcwd(buf, MAXPATHLEN) == NULL)
	return(0);

    /* Are we in Kansas, Toto? */
    vio.in = 0;
    vio.in_size = 0;
    vio.out = piobuf;
    vio.out_size = CFS_PIOBUFSIZE;
    rc = pioctl(buf, _VICEIOCTL(_VIOC_GETFID), &vio, 0);
    return(rc == 0);
}

static int simple_pioctl(char *path, unsigned char opcode, int follow)
{
    struct ViceIoctl vio;

    vio.in = 0;
    vio.in_size = 0;
    vio.out = 0;
    vio.out_size = 0;

    return pioctl(path, _VICEIOCTL(opcode), &vio, follow);
}

static void CheckVolumes(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc != 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOCCKBACK, 1);
    if (rc < 0) { PERROR("VIOC_VIOCCKBACK"); exit(-1); }
}

static void ClearPriorities(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc != 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOC_CLEARPRIORITIES, 0);
    if (rc < 0) { PERROR("  VIOC_CLEARPRIORITIES"); exit(-1); }
}

static void Redir (int argc, char *argv[], int opslot)
{
    int rc;
    struct ViceIoctl vio;
    struct in_addr whereto = { INADDR_ANY };

    if (argc != 3 && argc != 4) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    /* Set up parms to pioctl */
    if (argc == 4)
	(void)parseHost(argv[3], &whereto);

    vio.in = (char *)&whereto;
    vio.in_size = sizeof(struct in_addr);
    vio.out_size = 0;
    vio.out = NULL;

    /* Do the pioctl */
    rc = pioctl(argv[2], _VICEIOCTL(_VIOC_REDIR), &vio, 1);
    if (rc <0) { PERROR("VIOC_REDIR"); }
}



static void Disconnect(int argc, char *argv[], int opslot)
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
	int ix = (int) (hcount * sizeof(unsigned long) + sizeof(int));
	struct in_addr host;

	if (!parseHost(argv[i], &host)) continue;

	*((unsigned long *) &insrv[ix]) = ntohl(host.s_addr);
	hcount++;
    }
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(unsigned long) + sizeof(int));
    }

    rc = pioctl(NULL, _VICEIOCTL(_VIOC_DISCONNECT), &vio, 1);
    if (rc < 0) { PERROR("VIOC_DISCONNECT"); exit(-1);}

    if (insrv)
	free(insrv);
}

static void ExamineClosure(int argc, char *argv[], int opslot)
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


static int doclosure(char *cloname, enum closure_ops opcode, int flags)
/* Code adapted from JJK's original implementations of {Examine,Replay}Closure
   Performs specified operation on cloname and prints results on stdout
   Returns 0 on success or non-fatal error, -1 on fatal error */
{
    int rc = 0;
    char volname[V_MAXVOLNAMELEN];
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
    if (!getcwd(cwd, MAXPATHLEN)) { PERROR("getcwd");return(-1);}
    if (chdir(volrootpath) < 0) { PERROR(volrootpath); return(0);}

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
	    if (rc < 0) { PERROR(cloname); return(-1);}
	}
	break;

    default:
	printf("Unknown opcode\n");
	return(-1);
    }

    /* Return home */
    if (chdir(cwd) < 0) { PERROR(cwd); return(-1);}

    return(0);
}


static int findclosures(char ***clist)
{/* Constructs an argv[]-like structure of closures in /usr/coda/spool/<uid>.
    Return the # of closures found.
  */
    int n = 0;
    int len;
    char *checkpointdir = NULL;
    char spooldir[MAXPATHLEN];
    DIR *dirp;
    struct dirent *td;

    CODACONF_STR(checkpointdir, "checkpointdir", "/usr/coda/spool");
    sprintf(spooldir, "%s/%d", checkpointdir, getuid());

    dirp = opendir(spooldir);
    if (dirp == NULL){ PERROR(spooldir); exit(-1);}

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
	len = strlen(td->d_name);
	(*clist)[n-1] = (char *)malloc(strlen(spooldir) + 1 + len + 1);
	strcpy((*clist)[n-1], spooldir);
	strcat((*clist)[n-1], "/");
	strcat((*clist)[n-1], td->d_name);
    }

    if (n == 0) printf("No closures found in %s\n", spooldir);
    return(n);
}


static int validateclosurespec(char *name, char *volname, char *volrootpath)
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
    vio.out_size = CFS_PIOBUFSIZE;
    rc = pioctl(volrootpath, _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
    if (rc) { PERROR(volrootpath); return(0);}

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
    vio.out_size = CFS_PIOBUFSIZE;
    rc = pioctl(volrootpath, _VICEIOCTL(_VIOC_GETFID), &vio, 1);
    if (rc) { PERROR(volrootpath); return(0);}
    fid = (ViceFid *)piobuf;
    if (fid->Vnode != 1 || fid->Unique != 1)
    {
	printf("%s is not the root of volume %s\n", volrootpath, volname);
	return(0);
    }

    return(1);
}

static void FlushCache(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc != 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOC_FLUSHCACHE, 1);
    if (rc < 0) { PERROR("VIOC_FLUSHCACHE"); exit(-1);}
}

static void FlushObject(int argc, char *argv[], int opslot)
{
    int i, w, rc;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */

	rc = simple_pioctl(argv[i], _VIOCFLUSH, 0);
	if (rc < 0)
	{
	    fflush(stdout);
	    if (errno == EMFILE) {
		fflush(stdout);
		fprintf(stderr, "Can't flush active file\n");
	    }
	    else PERROR("VIOCFLUSH");
	    continue;
	}
	else {if (argc > 3) printf("\n");}
    }
}


static void FlushVolume(int argc, char *argv[], int opslot)
{
    int i, w, rc;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */

	rc = simple_pioctl(argv[i], _VIOC_FLUSHVOLUME, 0);
	if (rc < 0) { PERROR("  VIOC_FLUSHVOLUME"); continue;}
	else {if (argc > 3) printf("\n");}
    }
}

static void BeginRepair(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

#ifdef __CYGWIN32__
    /* test if the filename ends in ".$cf" */
    int len = strlen(argv[2]);
    if (len >= 4 && (argv[2][len-3] == '$' && argv[2][len-2] == 'c' &&
		     argv[2][len-1] == 'f' && argv[2][len-4] == '.')) {
	/* if so, strip the '.$cf'. Otherwise venus doesn't understand what
	 * file we're talking about */
	argv[2][len-4] = '\0';
    }
#endif

    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = CFS_PIOBUFSIZE;
    vio.out = piobuf;
    memset(piobuf, 0, CFS_PIOBUFSIZE);

    rc = pioctl(argv[2], _VICEIOCTL(_VIOC_ENABLEREPAIR), &vio, 0);
    if (rc < 0) {
	if (errno == EOPNOTSUPP) {
	    fflush(stdout);
	    fprintf(stderr, "%s isn't inconsistent\n", argv[2]);
	} else
	    PERROR("VIOC_ENABLEREPAIR");

	exit(-1);
    }
}

static void DisableASR(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = CFS_PIOBUFSIZE;
    vio.out = piobuf;
    memset(piobuf, 0, CFS_PIOBUFSIZE);

    rc = pioctl(argv[2], _VICEIOCTL(_VIOC_DISABLEASR), &vio, 0);
    if (rc < 0) { PERROR("VIOC_DISABLEASR"); exit(-1); }
}

static void EnableASR(int argc, char *argv[], int opslot)
{
    struct ViceIoctl vio;
    int rc;

    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = CFS_PIOBUFSIZE;
    vio.out = piobuf;
    memset(piobuf, 0, CFS_PIOBUFSIZE);

    rc = pioctl(argv[2], _VICEIOCTL(_VIOC_ENABLEASR), &vio, 0);
    if (rc < 0) { PERROR("VIOC_ENABLEASR"); exit(-1); }
}

static void EndRepair(int argc, char *argv[], int opslot) 
{
    struct ViceIoctl vio;
    int rc;
    if (argc != 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    vio.in_size = 1 + (int) strlen(argv[2]);
    vio.in = argv[2];
    vio.out_size = CFS_PIOBUFSIZE;
    vio.out = piobuf;
    memset(piobuf, 0, CFS_PIOBUFSIZE);

    rc = pioctl(argv[2], _VICEIOCTL(_VIOC_DISABLEREPAIR), &vio, 0);
    if (rc < 0) { PERROR("VIOC_DISABLEREPAIR"); exit(-1);}
}

static void FlushASR(int argc, char *argv[], int opslot) {
    int i, rc, w;
    struct ViceIoctl vio;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)  {
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if >1 fid */
	/* Get its path */
	vio.in = 0;
	vio.in_size = 0;
	vio.out = piobuf;
	vio.out_size = CFS_PIOBUFSIZE;
	memset(piobuf, 0, CFS_PIOBUFSIZE);

	rc = pioctl(argv[i], _VICEIOCTL(_VIOC_FLUSHASR), &vio, 0);
	if (rc < 0) { PERROR("VIOC_FLUSHASR"); continue; }
	printf("\n");
    }
}

static int pioctl_GetFid(char *path, ViceFid *fid, char *realm, ViceVersionVector *vv)
{
    struct GetFid {
	ViceFid           fid;
	ViceVersionVector vv;
	char		  realm[MAXHOSTNAMELEN+1];
    } out;
    struct ViceIoctl vio;
    int              rc;

    memset(&out, 0, sizeof(out));

    vio.in = 0;
    vio.in_size = 0;
    vio.out = (char *)&out;
    vio.out_size = sizeof(out);

#ifdef __CYGWIN32__
    rc = pioctl(path, 8972, &vio, 0);
#else
    rc = pioctl(path, _VICEIOCTL(_VIOC_GETFID), &vio, 0);
#endif

    if (rc < 0) return rc;

    /* Got the fid! */
    if (fid)   memcpy(fid, &out.fid, sizeof(ViceFid));
    if (realm) strcpy(realm, out.realm);
    if (vv)    memcpy(vv,  &out.vv,  sizeof(ViceVersionVector));

    return rc;
}


static void GetFid(int argc, char *argv[], int opslot)
{
    int i, rc, w;
    ViceFid fid;
    char realmname[MAXHOSTNAMELEN+1];
    ViceVersionVector vv;
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

	rc = pioctl_GetFid(argv[i], &fid, realmname, &vv);
	if (rc < 0) { PERROR("VIOC_GETFID"); continue; }

	sprintf(buf, "%x.%x.%x@%s", (unsigned int)fid.Volume,
		(unsigned int)fid.Vnode, (unsigned int)fid.Unique, realmname);
	printf("FID = %-20s  ", buf);
	SPrintVV(buf, sizeof(buf), &vv);
	printf("VV = %-24s\n", buf);
    }
}

static void GetPFid(int argc, char *argv[], int opslot)
{
    int i, rc, w;
    struct ViceIoctl vio;
    ViceFid fid;
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
	char tmp;
	if (sscanf(argv[i], "%x.%x.%x@%c", &fid.Volume, &fid.Vnode, &fid.Unique, &tmp) != 4)
	{
	    printf("Malformed fid: should look like %%x.%%x.%%x@<realm>\n");
	    continue;
	}

	/* Get its path */
	char *realmname = strrchr(argv[i], '@')+1;
	memcpy(piobuf, &fid, sizeof(ViceFid));
	strcpy(piobuf + sizeof(ViceFid), realmname);
	vio.in = piobuf;
	vio.in_size = sizeof(ViceFid) + strlen(realmname) + 1;
	vio.out = (char *)&fid;
	vio.out_size = sizeof(fid);

	rc = pioctl(NULL, _VICEIOCTL(_VIOC_GETPFID), &vio, 0);
	if (rc < 0) { PERROR("VIOC_GETPFID"); continue; }

	sprintf(buf, "%x.%x.%x", (unsigned int)fid.Volume,
		(unsigned int)fid.Vnode, (unsigned int)fid.Unique);
	printf("FID = %-20s\n", buf);
    }
}

static int pioctl_SetVV(char *path, ViceVersionVector *vv)
{
    struct ViceIoctl vio;
    int              rc;

    vio.in = (char *)vv;
    vio.in_size = sizeof(ViceVersionVector);
    vio.out = 0;
    vio.out_size = 0;

#ifdef __CYGWIN32__
    rc = pioctl(path, 8974, &vio, 0);
#else
    rc = pioctl(path, _VICEIOCTL(_VIOC_SETVV), &vio, 0);
#endif
    return rc;
}

static void ExpandObject(int argc, char *argv[], int opslot)
{
    int i, w;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	/* echo input if more than one fid */
	if (argc > 3) printf("%s\n", argv[i]);

	if (simple_pioctl(argv[i], _VIOC_EXPANDOBJECT, 0) < 0)
	    { PERROR("VIOC_EXPANDOBJECT"); continue; }
    }
}

static void CollapseObject(int argc, char *argv[], int opslot)
{
    int i, w;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	/* echo input if more than one fid */
	if (argc > 3) printf("%s\n", argv[i]);

	if (simple_pioctl(argv[i], _VIOC_COLLAPSEOBJECT, 0) < 0)
	    { PERROR("VIOC_COLLAPSEOBJECT"); continue; }
    }
}

static void MarkFidIncon(int argc, char *argv[], int opslot)
{
    int i, w;
    ViceVersionVector vv;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	/* echo input if more than one fid */
	if (argc > 3) printf("%s\n", argv[i]);

	/* Validate next fid */
	if (pioctl_GetFid(argv[i], NULL, NULL, &vv) < 0)
	{ PERROR("VIOC_GETFID"); continue; }

	SetIncon(vv);

	if (pioctl_SetVV(argv[i], &vv) < 0)
	{ PERROR("VIOC_SETVV"); continue; }
    }
}

static void GetPath(int argc, char *argv[], int opslot)
{
    int i, rc, w;
    struct ViceIoctl vio;
    ViceFid fid;

    if (argc < 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one fid */
	/* Validate next fid */
	char tmp;
	if (sscanf(argv[i], "%x.%x.%x@%c", &fid.Volume, &fid.Vnode, &fid.Unique, &tmp) != 4)
	{
	    printf("Malformed fid: should look like %%x.%%x.%%x@<realm>\n");
	    continue;
	}

	/* Get its path */
	char *realmname = strrchr(argv[i], '@')+1;
	memcpy(piobuf, &fid, sizeof(ViceFid));
	strcpy(piobuf + sizeof(ViceFid), realmname);
	vio.in = piobuf;
	vio.in_size = sizeof(ViceFid) + strlen(realmname) + 1;
	vio.out = piobuf;
	vio.out_size = CFS_PIOBUFSIZE;
	rc = pioctl(NULL, _VICEIOCTL(_VIOC_GETPATH), &vio, 0);
	if (rc < 0) { PERROR("VIOC_GETPATH"); continue; }
	printf("\t%s\n", vio.out);
    }
}

static void Help(int argc, char *argv[], int opslot)
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


static void ListACL(int argc, char *argv[], int opslot)
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
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;
	rc = pioctl(argv[i], _VICEIOCTL(_VIOCGETAL), &vio, 1);
	if (rc < 0) { PERROR(argv[i]); continue; }

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

static int parseacl(char *s, struct acl *a)
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
	CODA_ASSERT(a->plusentries);
    }
    sscanf(c, "%d", &a->minuscount);
    c += strlen(c) + 1;
    if (a->minuscount > 0)
    {
	a->minusentries = (struct aclentry *) calloc(a->minuscount, sizeof(struct aclentry));
	CODA_ASSERT(a->minusentries);
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


static void fillrights(int x, char *s)
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

static int getrights(char *s, int *x)
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


static void translate(char *s, char oldc, char newc)
/* Changes every occurence of oldc to newc in s */
{
    int i, size;

    size = (int) strlen(s);
    for (i = 0; i < size; i++)
	if (s[i] == oldc) s[i] = newc;
}

static void GetMountPoint(int argc, char *argv[], int opslot)
{
    int i, rc;
    struct ViceIoctl vio;
    struct {
	VolumeId volume;
	char realm[MAXHOSTNAMELEN+1];
    } arg;

    memset(&arg, 0, sizeof(arg));

    /* Parse command line arguments. */
    if (argc < 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    for (i = 2; i < argc; i++) {
	rc = sscanf(argv[i], "%x@%s", (unsigned int *)&arg.volume, arg.realm);
	if (rc < 2) {
	    printf("Usage: %s\n", cmdarray[opslot].usetxt);
	    exit(-1);
	}

	vio.in = (char *)&arg;
	vio.in_size = sizeof(arg);
	vio.out = piobuf;
	vio.out_size = CFS_PIOBUFSIZE;

	/* Do the pioctl */
	rc = pioctl(NULL, _VICEIOCTL(_VIOC_GET_MT_PT), &vio, 1);
	if (rc < 0) { PERROR("Failed in GetMountPoint."); exit(-1); }

	/* Print output field */
	printf("%s:  %s\n", argv[i], (char *)piobuf);
    }
}

static void ListCache(int argc, char *argv[], int opslot)
{
    int i, rc = 0;

    int long_format = 0;          /* If == 1, list in a long format. */
    unsigned int valid = 0;       /* list the following fsobjs, if
1: only valid, 2: only non-valid, 3: all */
    int file_specified = 0;       /* If == 0, list result to stdout. */
    int vol_specified = 0;        /* If == 0, all volumes cache status are listed. */
    char *filename = (char *)0;           /* Specified output file. */

    const int max_line = 256;
    char  *venus_file = "/tmp/_Venus_List_Cache"; /* Output file by Venus. */

    struct listcache_in {
	char fname[23];     /* strlen("/tmp/_Venus_List_Cache")+1 */
	int  first_volume;  /* if 1, Venus will unlink the file specified fname. */
	int  long_format;   /* = long_format */
	int  valid;         /* = valid */
    } data;

    struct ViceIoctl vio;
    char buf[max_line];

    /* Parse command line arguments. */
    if (argc < 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }
    int argi = 0;                     /* Index for argv. */
    if (argc < 3) {
	file_specified = 0;
	vol_specified = 0;
	long_format = 0;
	valid = 3;          /* all fsobjs to be listed. */
    } else {
	for (argi = 2; (argi < argc) && (argv[argi][0] == '-'); argi++) {
	    if (strcmp(argv[argi], "-f") == 0 || strcmp(argv[argi], "-file") == 0) {
		if ( (argi + 1) < argc ) {
		    file_specified = 1;
		    filename = argv[++argi];
		} else {                /* filename is not specified as argv[argi+1]. */
		    printf("Usage: %s\n", cmdarray[opslot].usetxt);
		    exit(-1);
		}
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
	    if ( sscanf(argv[i], "%x", (unsigned int *)&vol_id) == 1 ) {
		vio.in = (char *)&vol_id;
		vio.in_size = (int) sizeof(VolumeId);
		vio.out = piobuf;
		vio.out_size = CFS_PIOBUFSIZE;
		memset(piobuf, 0, CFS_PIOBUFSIZE);

		/* Do the pioctl getting mount point pathname */
		rc = pioctl(NULL, _VICEIOCTL(_VIOC_GET_MT_PT), &vio, 1);
		if (rc < 0) { PERROR("Failed in GetMountPoint."); exit(-1); }
		strcpy(mtptpath, piobuf);
	    }

	    strcpy(data.fname, venus_file);
	    data.first_volume = ((i == argi) ? 1 : 0);
	    data.long_format = long_format;
	    data.valid = valid;
	    vio.in = (char *)&data;
	    vio.in_size = (int) sizeof(struct listcache_in);
	    vio.out_size = CFS_PIOBUFSIZE;
	    vio.out = piobuf;
	    memset(piobuf, 0, CFS_PIOBUFSIZE);

	    /* Do the pioctl */
	    if (vol_id)       /* VolumeId is specified. */
		rc = pioctl(mtptpath, _VICEIOCTL(_VIOC_LISTCACHE_VOLUME), &vio, 1);
	    else              /* Mount point pathname is specified. */
		rc = pioctl(argv[i], _VICEIOCTL(_VIOC_LISTCACHE_VOLUME), &vio, 1);
	    if (rc < 0) { PERROR(argv[i]); exit(-1); }
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

	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;
	memset(piobuf, 0, CFS_PIOBUFSIZE);
	/* Do the pioctl */
	rc = pioctl(NULL, _VICEIOCTL(_VIOC_LISTCACHE), &vio, 1);
	if (rc < 0) { PERROR("Failed in ListCache."); exit(-1); }
    }

    /* List Cache Status.
     * cat the contens of venus_file to stdout or a specified file.
     */
    FILE *src_fp;
    FILE *dest_fp = stdout;
    if (file_specified) {
	dest_fp = fopen(filename, "w");
	if (!dest_fp) {
	    printf("Cannot open file: %s\n", filename);
	    exit(-1);
	}
    }
    src_fp = fopen(venus_file, "r");
    if (!src_fp) {
	printf("Cannot open file: %s\n", filename);
	if (file_specified)
	    fclose(dest_fp);
	exit(-1);
    }

    while (rc != EOF && fgets(buf, max_line, src_fp) != NULL )
	rc = fputs(buf, dest_fp);

    fclose(src_fp);
    if (file_specified)
	fclose(dest_fp);
    if (rc == EOF)
	exit(-1);
}


static void ListVolume(int argc, char *argv[], int opslot)
{
    int i, rc;
    struct ViceIoctl vio;
    VolumeStatus *vs;
    char *volname, *omsg, *motd;
    VolumeStateType conn_state;
    int conflict, cml_count;
    unsigned int age, hogtime;
    uint64_t cml_bytes;
    char *ptr;
    int local_only = 0;

    if (argc < 3)
    {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    for (i = 2; i < argc; i++)
    {
	if (strcmp(argv[i], "-local") == 0) {
	    local_only = 1;
	    continue;
	}
	/* Set up parms to pioctl */
	vio.in = (char *)&local_only;
	vio.in_size = sizeof(int);
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;

	/* Do the pioctl */
	rc = pioctl(argv[i], _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
	if (rc <0) { PERROR(argv[i]); continue; }

	/* Get pointers to output fields */
	/* Format is (status, name, conn_state, conflict,
	   cml_count, offlinemsg, motd, age, hogtime, cml_bytes) */
	ptr = piobuf;		/* invariant: ptr always point to next obj
				   to be read */
	vs = (VolumeStatus *)ptr;
	ptr += sizeof(VolumeStatus);
	volname = ptr; ptr += strlen(volname)+1;

	conn_state = (VolumeStateType)*(int32_t *)ptr;
	ptr += sizeof(int32_t);
	conflict = *(int32_t *)ptr;
	ptr += sizeof(int32_t);
	cml_count = *(int32_t *)ptr;
	ptr += sizeof(int32_t);

	omsg = ptr; ptr += strlen(omsg)+1;
	motd = ptr; ptr += strlen(motd)+1;

	age = *(uint32_t *)ptr;
	ptr += sizeof(uint32_t);
	hogtime	= *(uint32_t *)ptr;
	ptr += sizeof(uint32_t);
	cml_bytes = *(uint64_t *)ptr;
	ptr += sizeof(uint64_t);

	/* Print output fields */
	if (argc > 3) printf("  %s:\n", argv[i]);  /* print directory name if more than one */
	printf("  Status of volume %08x (%u) named \"%s\"\n",
	       vs->Vid, vs->Vid, volname);
	if (*omsg) printf("  Offline message is \"%s\"\n", omsg);
	if (*motd) printf("  Message of the day is \"%s\"\n", motd);
	printf("  Volume type is %s\n", xlate_vvtype(vs->Type));
	printf("  Connection State is %s\n", print_conn_state(conn_state));
	printf("  Reintegration age: %u sec, hogtime %.3f sec\n",
	       age, hogtime / 1000.0);
	/* info not avail if disconnected, or if we did a local query */
	if (conn_state!=Emulating && local_only == 0) {
	    printf("  Minimum quota is %u,", vs->MinQuota);
	    if (vs->MaxQuota > 0)
		printf(" maximum quota is %u\n", vs->MaxQuota);
	    else printf(" maximum quota is unlimited\n");
	    printf("  Current blocks used are %u\n", vs->BlocksInUse);
	    printf("  The partition has %u blocks available out of %u\n",
		   vs->PartBlocksAvail, vs->PartMaxBlocks);
	}
	if (conflict)
	    printf("  *** There are pending conflicts in this volume ***\n");
	if (conn_state == Logging || conn_state == Emulating)
	    printf("  There are %d CML entries pending for reintegration (%llu bytes)\n", cml_count, cml_bytes);
	printf("\n");
    }


}


static void LookAside(int argc, char *argv[], int opslot)
{
    int i, rc, spaceleft;
    struct ViceIoctl vio;

    if (argc < 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    /* Create a command string of all text after "cfs lka" */
    memset(piobuf, 0, CFS_PIOBUFSIZE);
    spaceleft = CFS_PIOBUFSIZE - 1; /* don't forget string terminator */
    for (i = 2; i < argc; i++) {
	strncat(piobuf, " ", spaceleft--); /* nop if piobuf full */
	strncat(piobuf, argv[i], spaceleft); /* nop if piobuf full */
	spaceleft -= strlen(argv[i]);
    }

    /* Pass the command string to the lka module */
    vio.in = piobuf;
    vio.in_size = strlen(piobuf) + 1;
    vio.out = piobuf;
    vio.out_size = CFS_PIOBUFSIZE;

    rc = pioctl(NULL, _VICEIOCTL(_VIOC_LOOKASIDE), &vio, 0);
    if (rc < 0) { PERROR("VIOC_LOOKASIDE"); return;}

    if (piobuf[0]) printf("%s", piobuf); /* result of lka command */
}

static char *myrealpath(const char *path, char *buf, size_t size)
{
    char curdir[MAXPATHLEN+1], *s;
    int rc;

    s = getcwd(curdir, sizeof(curdir));
    if (!s) return NULL;

    rc = chdir(path);
    if (rc < 0) return NULL;

    s = getcwd(buf, size);

    chdir(curdir);
    return s;
}


static void LsMount (int argc, char *argv[], int opslot)
/* This code will not detect a mount point where the root
   directory of the mounted volume denies permission for
   a chdir().  Hopefully this will be a rare event.
 */
{
    int i, rc, w;
    struct ViceIoctl vio;
    char path[MAXPATHLEN+1], tail[MAXNAMLEN+1];
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
		if (rc < 0) { PERROR("readlink"); continue; }
		/* Check if first char is a '#'; this is a heuristic, but rarely misleads */
		if (piobuf[0] == '#')
		    printf("Dangling sym link; looks like a mount point for volume \"%s\"\n", &piobuf[1]);
		else printf("!! %s is not a volume mount point\n", argv[i]);
		continue;
	    }
	}

	/* Next see if you can chdir to the target */
	s = myrealpath(argv[i], path, sizeof(path));
	if (!s) {
	    if (errno == ENOTDIR)
		fprintf(stderr, "%s - Not a mount point\n", argv[i]);
	    else
		PERROR("realpath");
	    continue;
	}

	/* there must be a slash in what myrealpath() returns */
	s = rindex(path, '/');
	*s = 0; /* lop off last component */
	strncpy(tail, s+1, sizeof(tail)); /* and copy it */

	/* Ask Venus if this is a mount point */
	vio.in = tail;
	vio.in_size = (int) strlen(tail)+1;
	vio.out = piobuf;
	vio.out_size = CFS_PIOBUFSIZE;
	memset(piobuf, 0, CFS_PIOBUFSIZE);
	rc = pioctl(path, _VICEIOCTL(_VIOC_AFS_STAT_MT_PT), &vio, 0);
	if (rc < 0)
	{
	    if (errno == EINVAL || errno == ENOTDIR) {printf("Not a mount point\n"); continue;}
	    else { PERROR("VIOC_AFS_STAT_MT_PT"); continue; }
	}

	printf("Mount point for volume \"%s\"\n", &piobuf[1]);
    }
}


static void MkMount (int argc, char *argv[], int opslot)
{
    int rc;
    struct ViceIoctl vio;
    char *dir, *entry, *vol = "";
    char buf[MAXPATHLEN+2];

    switch (argc)
    {
    case 4:   vol = argv[3];
    case 3:   dir = argv[2]; break;

    default: printf("Usage: %s\n", cmdarray[opslot].usetxt); exit(-1);
    }

    entry = strrchr(dir, '/');
    if (!entry) {
	entry = dir;
	dir = ".";
    } else {
	*entry = '\0';
	entry++;
    }

    sprintf(buf, "%s/%s", entry, vol);
    vio.in_size = strlen(entry) + strlen(vol) + 2;
    vio.in = buf;
    vio.out_size = 0;
    vio.out = 0;
    rc = pioctl(dir, _VICEIOCTL(_VIOC_ADD_MT_PT), &vio, 1);
    if (rc < 0) { PERROR(dir); exit(-1); }
}

static void PurgeML(int argc, char *argv[], int opslot)
{
    int  rc;
    char *codadir;

    switch(argc)
    {
    case 3: codadir = argv[2]; break;

    default:
	    printf("Usage: %s\n", cmdarray[opslot].usetxt);
	    exit(-1);
    }


    rc = simple_pioctl(codadir, _VIOC_PURGEML, 1);
    if (rc) { PERROR("VIOC_PURGEML"); exit(-1); }
}


static void Reconnect(int argc, char *argv[], int opslot)
{
    int rc;
    struct ViceIoctl vio;
    char *insrv = NULL;

    if (argc < 2 || argc > 10) {
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
	int ix = (int) (hcount * sizeof(unsigned long) + sizeof(int));
	struct in_addr host;

	if (!parseHost(argv[i], &host)) continue;

	*((unsigned long *) &insrv[ix]) = ntohl(host.s_addr);
	hcount++;
    }
    if (hcount) {
	((int *) insrv)[0] = hcount;
	vio.in = insrv;
	vio.in_size = (int) (hcount * sizeof(unsigned long) + sizeof(int));
    }

    rc = pioctl(NULL, _VICEIOCTL(_VIOC_RECONNECT), &vio, 1);
    if (rc < 0) { PERROR("VIOC_RECONNECT"); exit(-1); }

    if (insrv)
	free(insrv);
}

static void ReplayClosure(int argc, char *argv[], int opslot)
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

static void RmMount(int argc, char *argv[], int opslot)
{
    int  i, rc, w, n;
    struct ViceIoctl vio;
    char *prefix, *suffix;

    if (argc < 3) {printf("Usage: %s\n", cmdarray[opslot].usetxt);  exit(-1);}

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {/* Remove one mount point per iteration */

	if (argc > 3) printf("  %*s  ", w, argv[i]); /* echo input if more than one */

	/* First set up the prefix and suffix correctly */
	prefix = argv[i];

	n = strlen(prefix) - 1;
	/* strip useless /'s at the end of prefix. */
	while (n >= 0 && prefix[n] == '/') {
	    prefix[n] = '\0'; n--;
	}

	suffix = rindex(prefix, '/');
	if (suffix) {
	    *suffix = 0; /* delimit the prefix */
	    suffix++;  /* and set the suffix pointer correctly */
	} else {
	    suffix = prefix;
	    prefix = ".";
	}

	/* Then do the pioctl */
	vio.in_size = (int) strlen(suffix) + 1;
	vio.in = suffix;
	vio.out_size = 0;
	vio.out = 0;
	rc = pioctl(prefix, _VICEIOCTL(_VIOC_AFS_DELETE_MT_PT), &vio, 0);
	if (rc) { PERROR("VIOC_AFS_DELETE_MT_PT"); continue; }
	else {if (argc > 3) printf("\n");}
    }
}

static void SetACL (int argc, char *argv[], int opslot)
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
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;
	rc = pioctl(dir, _VICEIOCTL(_VIOCGETAL), &vio, 1);
	if (rc <0) { PERROR(dir); exit(-1); }
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
	    if (strcasecmp((*aptr)[j].id, newid) == 0)
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
    rc = pioctl(dir, _VICEIOCTL(_VIOCSETAL), &vio, 1);
    if (rc < 0) {
	PERROR(dir);
	if (errno == EINVAL)
	    fputs("*** User unknown by the servers?\n", stderr);
    }
}


static void SetVolume   (int argc, char *argv[], int opslot) {printf("Not supported by Coda yet\n");}


static void SetQuota    (int argc, char *argv[], int opslot) 
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
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;

	/* Do the pioctl */
	rc = pioctl(argv[i], _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
	if (rc <0) { PERROR(argv[i]); return; }

	/* Get pointers to output fields */
	vs = (VolumeStatus *)piobuf;
	printf("maximum quota is %u\n", vs->MaxQuota);

	vs->MaxQuota = atoi(argv[i+1]);
	printf("New value of vs->MaxQuota will be set to %u\n", vs->MaxQuota);

	vio.in_size  = CFS_PIOBUFSIZE;
	vio.in       = piobuf;
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out      = piobuf;

	rc = pioctl(argv[i], _VICEIOCTL(_VIOCSETVOLSTAT), &vio, 1);
	if (rc <0) { PERROR("Setting new quota"); return; }
    }
}

static void Strong(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc < 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOC_STRONG, 1);
    if (rc < 0){ PERROR("VIOC_STRONG"); exit(-1); }
}

static void Adaptive(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc < 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOC_ADAPTIVE, 1);
    if (rc < 0){ PERROR("VIOC_ADAPTIVE"); exit(-1); }
}

static void TruncateLog(int argc, char *argv[], int opslot)
{
    int rc;

    if (argc != 2) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    rc = simple_pioctl(NULL, _VIOC_TRUNCATELOG, 1);
    if (rc < 0) { PERROR("  VIOC_TRUNCATELOG"); exit(-1); }
}

static void UnloadKernel(int argc, char *argv[], int opslot)
{
    int rc;

    rc = simple_pioctl(NULL, _VIOC_UNLOADKERNEL, 0);
    if (rc < 0) { PERROR("  VIOC_UNLOADKERNEL"); exit(-1); }
}

static void WhereIs (int argc, char *argv[], int opslot)
{
    int rc, i, j, w;
    struct ViceIoctl vio;
    struct in_addr *custodians;
    struct hostent *hent;

    if (argc < 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++)
    {
	/* Set up parms to pioctl */
	vio.in = 0;
	vio.in_size = 0;
	vio.out_size = CFS_PIOBUFSIZE;
	vio.out = piobuf;

	/* Do the pioctl */
	rc = pioctl(argv[i], _VICEIOCTL(_VIOCWHEREIS), &vio, 1);
	if (rc <0) { PERROR(argv[i]); continue; }

	/* Print custodians */
	if (argc > 3) printf("  %*s:  ", w, argv[i]);
	/* pioctl returns array of IP addrs */
	custodians = (struct in_addr *)piobuf;
	for (j = 0; j < 8; j++)
	{
	    if (custodians[j].s_addr == 0) continue;
	    hent = gethostbyaddr((char *)&custodians[j], sizeof(long), AF_INET);
	    if (hent) printf("  %s", hent->h_name);
	    else      printf("  %s", inet_ntoa(custodians[j]));
	}
	printf("\n");
    }
}


static void WaitForever (int argc, char *argv[], int opslot)
{
    int rc;
    struct ViceIoctl vio;
    struct {
	int value;
	char realm[MAXHOSTNAMELEN+1];
    } arg;

    memset(&arg, 0, sizeof(arg));
    arg.value = -1;

    if (argc > 3) {
	strncpy(arg.realm, argv[3], MAXHOSTNAMELEN);
	arg.realm[MAXHOSTNAMELEN] = '\0';
    }

    if (argc > 2) {
	if (strcmp(argv[2], "-on") == 0) arg.value = 1;
	if (strcmp(argv[2], "-off") == 0) arg.value = 0;
    }

    if (arg.value == -1) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    vio.in = (char *)&arg;
    vio.in_size = sizeof(arg);
    vio.out = 0;
    vio.out_size = 0;
    rc = pioctl(NULL, _VICEIOCTL(_VIOC_WAITFOREVER), &vio, 0);
    if (rc < 0){ PERROR("VIOC_WAITFOREVER"); exit(-1); }
}


static void WriteDisconnect(int argc, char *argv[], int opslot)
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

    if (argc == i)      /* no more args -- do them all */
    {
	rc = pioctl(NULL, _VICEIOCTL(_VIOC_WD_ALL), &vio, 1);
	if (rc) { PERROR("VIOC_WD_ALL"); exit(-1); }
    } else {
	w = getlongest(argc, argv);
	for (int j = i; j < argc; j++)
	{
	    if (argc > i+1) printf("  %*s\n", w, argv[j]); /* echo input if more than one fid */

	    rc = pioctl(argv[j], _VICEIOCTL(_VIOC_BEGINML), &vio, 0);
	    if (rc < 0) { PERROR("VIOC_BEGINML"); exit(-1); }
	}
    }
}

static void ForceReintegrate(int argc, char *argv[], int opslot)
{
    int  i = 2, rc, w;
    VolumeStatus *vs;
    char *volname;
    int conflict;
    int cml_count;
    char *ptr;

    if (argc < 3) {
	printf("Usage: %s\n", cmdarray[opslot].usetxt);
	exit(-1);
    }

    w = getlongest(argc, argv);

    for (i = 2; i < argc; i++) {
	if (argc > 3) printf("  %*s:  ", w, argv[i]); /* echo input if more than one fid */

	rc = simple_pioctl(argv[i], _VIOC_SYNCCACHE, 0);
	fflush(stdout);
	if (rc < 0) {
	    PERROR("VIOC_SYNCCACHE");
	    fprintf(stderr, "  VIOC_SYNCCACHE returns %d\n",rc);
	} else {   /* test CML entries remaining by doing a ListVolume*/
	    struct ViceIoctl vio;

	    vio.in = 0;
	    vio.in_size = 0;
	    vio.out_size = CFS_PIOBUFSIZE;
	    vio.out = piobuf;

	    /* Do the pioctl */
	    rc = pioctl(argv[i], _VICEIOCTL(_VIOCGETVOLSTAT), &vio, 1);
	    if (rc < 0) {
		PERROR("VIOC_GETVOLSTAT");
		fprintf(stderr, "  VIOC_GETVOLSTAT returns %d\n", rc);
	    }
	    else {  /* Get pointers to output fields */
		/* Format is (status, name, conn_state, conflict,
		   cml_count, offlinemsg, motd) */
		ptr = piobuf;		/* invariant: ptr always point to next obj
					   to be read */
		vs = (VolumeStatus *)ptr;
		ptr += sizeof(VolumeStatus);
		volname = ptr;
		ptr += strlen(volname)+1;
		ptr += sizeof(int);
		memcpy ((void *)&conflict, (void *)ptr, sizeof(int));
		ptr += sizeof(int);
		memcpy ((void *)&cml_count, (void *)ptr, sizeof(int));

		if (!cml_count)
		    printf("Modifications to %s reintegrated to server\n",volname);
		else {
		    printf("%d CML entries remaining for volume %s\n",cml_count,volname);
		    if (conflict)
			printf("Reintegration failed due to a conflict\n");
		}
	    }
	}
    }
}

static void WriteReconnect(int argc, char *argv[], int opslot)
{
    int  i, w, rc;

    if (argc == 2) {      /* do them all */
	rc = simple_pioctl(NULL, _VIOC_WR_ALL, 1);
	if (rc) { PERROR("VIOC_WR_ALL"); exit(-1); }
    } else {
	w = getlongest(argc, argv);
	for (i = 2; i < argc; i++)
	{
	    if (argc > 3) printf("  %*s\n", w, argv[i]); /* echo input if more than one fid */

	    rc = simple_pioctl(argv[i], _VIOC_ENDML, 1);
	    if (rc) { PERROR("VIOC_ENDML"); exit(-1); }
	}
    }
}

static void At_SYS(int argc, char *argv[], int opslot)
{
    printf("%s\n", SYSTYPE);
}

static void At_CPU(int argc, char *argv[], int opslot)
{
    printf("%s\n", CPUTYPE);
}

static int findslot(char *s)
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

static char *xlate_vvtype(ViceVolumeType vvt)
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

static char *print_conn_state(VolumeStateType conn_state)
{
    switch(conn_state) {
    case Hoarding: return("Connected");
    case Emulating: return("Disconnected");
    case Logging: return("WriteDisconnected");
    case Resolving: return("Connected");
    default: return("????");
    }
}

static int getlongest(int argc, char *argv[])
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
 - because the queries sent to the DNS servers are timing out as
   well? Add your servers to /etc/hosts, and try again. -JH
*/
