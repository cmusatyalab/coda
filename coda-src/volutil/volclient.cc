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

/***************************************************/
/*	volclient.c				   */
/*	    - client side for volume utilities	   */
/***************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <lwp/timer.h>
#include <rpc2/rpc2.h>
#include <rpc2/se.h>
#include <rpc2/sftp.h>
#include <util.h>
#include <partition.h>
#include <ports.h>
#include <vice.h>
#include <callback.h>
#include <volutil.h>
#include <voldump.h>

#ifdef __cplusplus
}
#endif

#include <vice_file.h>
#include <cvnode.h>
#include <volume.h>

#include <codaconf.h>
#include <coda_config.h>
#include <vice_file.h>
#include <getsecret.h>

static char *serverconf = SYSCONFDIR "/server"; /* ".conf" */
static char *vicedir = NULL;
static int   nservers = 0;

static RPC2_EncryptionKey vkey;	/* Encryption key for bind authentication */
/* hack to make argc and argv visible to subroutines */
static char **this_argp;
static int these_args;

static char s_hostname[100];
static int timeout = 0;
static RPC2_Handle rpcid;
static long rc;

static void backup(void);
static void salvage(void);
static void create(void);
static void create_rep(void);
static void clone(void);
static void makevldb(void);
static void makevrdb(void);
static void info(void);
static void showvnode(void);
static void setvv(void);
static void purge(void);
static void lookup(void);
static void lock(void);
static void unlock(void);
static void updatedb(void);
static void shutdown(void);
static void swaplog(void);
static void swapmalloc(void);
static void setdebug(void);
static void dump(void);
static void dumpestimate(void);
static void restorefromback(void);
static void dumpmem(void);
static void rvmsize(void);
static void setlogparms(void);
static void markasancient(void);
static void timing(void);
static void tracerpc(void);
static void printstats(void);
static void getvolumelist(void);
static void showcallbacks(void);
static void truncatervmlog(void);
static void getmaxvol(void);
static void setmaxvol(void);
static void peekint(void);
static void pokeint(void);
static void peekmem(void);
static void pokemem(void);
static void peekxmem(void);
static void pokexmem(void);
static void setwb(RPC2_Integer wbflag);

#define ROCKTAG 12345
struct rockInfo {
    int fd;		    /* Open filedescriptor for ReadDump. */
    VolumeId volid;	    /* Volume being dumped. */
    unsigned long numbytes; /* Number of bytes already written to the file. */
};

static void V_InitRPC(int timeout);
static int V_BindToServer(char *fileserver, RPC2_Handle *RPCid);
static void VolDumpLWP(struct rockInfo *rock);
extern long volDump_ExecuteRequest(RPC2_Handle, RPC2_PacketBuffer*,SE_Descriptor*);

void ReadConfigFile(void)
{
    char    confname[MAXPATHLEN];

    /* don't complain if config files are missing */
    codaconf_quiet = 1;

    /* Load configuration file to get vice dir. */
    sprintf (confname, "%s.conf", serverconf);
    (void) conf_init(confname);

    CONF_STR(vicedir,		"vicedir",	   "/vice");
    CONF_INT(nservers,		"numservers", 	   1); 

    vice_dir_init(vicedir, 0);
}


int main(int argc, char **argv)
{
    /* Set the default timeout and server host */
    timeout = 30;	/* Default rpc2 timeout is 30 seconds. */
    gethostname(s_hostname, sizeof(s_hostname) -1);

    ReadConfigFile();
	
    while (argc > 2 && *argv[1] == '-') { /* Both options require 2 arguments. */
	if (strcmp(argv[1], "-h") == 0) { /* User specified other host. */
	    struct hostent *hp;
	    argv++; argc--;
	    hp = gethostbyname(argv[1]);
	    if (hp) {
		strcpy(s_hostname, hp->h_name);
		argv++; argc--;
	    } else {
		fprintf(stderr, "%s is not a valid host name.\n", argv[1]);
		exit(-1);
	    }
	}
	
	if ((argc > 2) && strcmp(argv[1], "-t") == 0) { 
	    /* User gave timeout */
	    timeout = atoi(argv[2]);
	    argv++; argc--;
	    argv++; argc--;
	}

	if ((argc > 2) && strcmp(argv[1], "-d") == 0) { 
	    /* User gave debuglevel */
	    RPC2_DebugLevel = atoi(argv[2]);
	    VolDebugLevel = atoi(argv[2]);
	    argv++; argc--;
	    argv++; argc--;
	}
    }

    if (argc < 2)
    	goto bad_options;

    CODA_ASSERT(s_hostname != NULL);
    V_InitRPC(timeout);
    V_BindToServer(s_hostname, &rpcid);

    this_argp = argv;
    these_args = argc;

    if (strcmp(argv[1], "ancient") == 0)
	markasancient();
    else if (strcmp(argv[1], "backup") == 0)
	backup();
    else if (strcmp(argv[1], "create") == 0)
	create();
    else if (strcmp(argv[1], "create_rep") == 0)
	create_rep();
    else if (strcmp(argv[1], "salvage") == 0)
	salvage();
    else if (strcmp(argv[1], "lock") == 0)
	lock();
    else if (strcmp(argv[1], "unlock") == 0)
	unlock();
    else if (strcmp(argv[1], "makevldb") == 0)
	makevldb();
    else if (strcmp(argv[1], "makevrdb") == 0)
	makevrdb();
    else if (strcmp(argv[1], "info") == 0)
	info();
    else if (strcmp(argv[1], "showvnode") == 0)
	showvnode();
    else if (strcmp(argv[1], "setvv") == 0)
	setvv();
    else if (strcmp(argv[1], "purge") == 0)
	purge();
    else if (strcmp(argv[1], "lookup") == 0)
	lookup();
    else if (strcmp(argv[1], "updatedb") == 0)
	updatedb();
    else if (strcmp(argv[1], "shutdown") == 0)
	shutdown();
    else if (strcmp(argv[1], "swaplog") == 0)
	swaplog();
    else if (strcmp(argv[1], "togglemalloc") == 0)
	swapmalloc();
    else if (strcmp(argv[1], "setdebug") == 0)
	setdebug();
    else if (strcmp(argv[1], "clone") == 0)
	clone();
    else if (strcmp(argv[1], "dump") == 0)
	dump();
    else if (strcmp(argv[1], "dumpestimate") == 0)
	dumpestimate();
    else if (strcmp(argv[1], "restore") == 0)
	restorefromback();
    else if (strcmp(argv[1], "dumpmem") == 0)
	dumpmem();
    else if (strcmp(argv[1], "rvmsize") == 0)
	rvmsize();
    else if (strcmp(argv[1], "setlogparms") == 0)
	setlogparms();
    else if (strcmp(argv[1], "timing") == 0)
	timing();
    else if (strcmp(argv[1], "tracerpc") == 0)
	tracerpc();
    else if (strcmp(argv[1], "printstats") == 0)
	printstats();
    else if (strcmp(argv[1], "getvolumelist") == 0)
	getvolumelist();
    else if (strcmp(argv[1], "showcallbacks") == 0)
	showcallbacks();
    else if (strcmp(argv[1], "truncatervmlog") == 0)
	truncatervmlog();
    else if (strcmp(argv[1], "getmaxvol") == 0)
        getmaxvol();
    else if (strcmp(argv[1], "setmaxvol") == 0)
	setmaxvol();
    else if (strcmp(argv[1], "peek") == 0)
	peekint();
    else if (strcmp(argv[1], "poke") == 0)
	pokeint();
    else if (strcmp(argv[1], "peeks") == 0)
	peekmem();
    else if (strcmp(argv[1], "pokes") == 0)
	pokemem();
    else if (strcmp(argv[1], "peekx") == 0)
	peekxmem();
    else if (strcmp(argv[1], "pokex") == 0)
	pokexmem();
    else if (strcmp(argv[1], "enablewb") == 0)
	setwb(1);
    else if (strcmp(argv[1], "disablewb") == 0)
	setwb(0);
    else
    	goto bad_options;

    return 0;

bad_options:
    fprintf(stderr,
"Usage: volutil [-h host] [-t timeout] [-d debuglevel] <option>,\n"
"    where <option> is one of the following:\n"
"\tancient, backup, create, create_rep, clone, dump, dumpestimate,\n"
"\trestore, info, lock, lookup, makevldb, makevrdb, purge, salvage,\n"
"\tsetvv, showvnode, shutdown, swaplog, setdebug, updatedb, unlock,\n"
"\tdumpmem, rvmsize, timing, enablewb, disablewb, printstats,\n"
"\tshowcallbacks, truncatervmlog,togglemalloc, getmaxvol, setmaxvol,\n"
"\tpeek, poke, peeks, pokes, peekx, pokex, setlogparms, tracerpc\n"
"\tgetvolumelist\n");
    exit(-1);
}

/**
 * ancient - mark backups successful
 * @groupid:	Replicated volume id
 * @repid:	Volume replica id
 *
 * Tell the server that backup succeeded for this volume. The next dump of this
 * volume, if incremental, will be based on the state represented by this
 * backup. The input should be in Hex.
 */
static void markasancient(void) 
{
    long backupid;
    
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil ancient <backupid>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%lX", &backupid) != 1){
	fprintf(stderr, "MarkAsAncient: Bad backupId %s\n", this_argp[2]);
	exit(-1);
    }

    rc = NewVolMarkAsAncient(rpcid, backupid);
    if (rc != RPC2_SUCCESS){
	fprintf(stderr, "VolMarkAsAncient failed with %s\n",
		RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    exit(0);	/* Funny, need to exit or the program never exits... */
}

/**
 * setlogparms - set volume recovery log parameters
 * @volid:	Volume replica id
 * @reson_flag:	Set resolution flag (should normally be set to 4)
 * @logsize_nentries:	Set size of the volume resolution log.
 *
 * Turn on resolution or change the log size for a volume. The volume ID can be
 * either the replicated ID or the non-replicated ID. Resolution is turned on
 * by specifying 4 after reson and can be turned off by specifying 0. The size
 * of the log can also be changed for the volume. The size parameter refers to
 * the number of maximum entries in the log. This should be a multiple of 32.
 * Typically this is set to 8192.
 */
static void setlogparms(void)
{
    long volid;
    long flag;
    long nentries;
    int i;

    nentries = 0;
    flag =  -1;
    
    if (these_args < 5) {
	fprintf(stderr, "Usage: volutil setlogparms <volid> reson <flag> logsize <nentries>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%lX", &volid) != 1) {
	fprintf(stderr, "setlogparms: Bad VolumeId %s\n", this_argp[2]);
	exit(-1);
    }
    for (i = 3; i < these_args ; i++) {
	if (strcmp(this_argp[i], "reson") == 0) {
	    i = i + 1;
	    if (sscanf(this_argp[i], "%ld", &flag) != 1) {
		fprintf(stderr, "Bad flag value %s\n", this_argp[i]);
		exit(-1);
	    }
	}
	if (strcmp(this_argp[i], "logsize") == 0) {
	    i = i + 1;
	    if (sscanf(this_argp[i], "%ld", &nentries) != 1) {
		fprintf(stderr, "Bad logsize value %s\n", this_argp[i]);
		exit(-1);
	    }
	}
    }
    
    rc = VolSetLogParms(rpcid, volid, flag, nentries);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSetLogParms failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Set Log parameters\n");
    exit(0);
}

/**
 * salvage
 *
 * The salvage option to volutil doesn't work right. Please don't try it.
 */
static void salvage(void)
{
    int err = 0;
    int debug = 0;			/* -d flag */
    int listinodeoption = 0;
    int forcesalvage = 0;
    VolumeId vid = 0;
    char *path = NULL;

    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil salvage [-d][-f][-i] partition "
			"[rw-vol number]\n"
			"The salvage option to volutil doesn't work right. "
			"Please don't try it.\n");
	exit(-1);
    }
    these_args--; this_argp++;
    these_args--; this_argp++;
    while (these_args && **this_argp == '-') {
	if (strcmp(*this_argp,"-d") == 0)
	    debug = 1;
	else if (strcmp(*this_argp,"-t") == 0) {
	    fprintf(stderr, "Testing option not implemented\n");
	    exit(1);
	}
	else if (strcmp(*this_argp,"-i") == 0)
	    listinodeoption = 1;
	else if (strcmp(*this_argp,"-f") == 0)
	    forcesalvage = 1;
	else
	    err = 1;
	these_args--;
	this_argp++;
    }
    if (err || these_args > 2) {
	fprintf(stderr, "Usage: volutil salvage [-d] [-f] [-i] partition [read/write-volume-number]\n");
	exit(-1);
    }
    if (these_args > 0)
	path = this_argp[0];
    if (these_args == 2) {
	if (sscanf(this_argp[1], "%lx", &vid) != 1){
	    fprintf(stderr, "salvage: invalid volume id specified; salvage aborted\n");
	    exit(1);
	}
    }

    rc = VolSalvage (rpcid, (RPC2_String)path, vid, forcesalvage, debug, listinodeoption);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSalvage failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Salvage complete.\n");
    exit(0);
}

static void stripslash(char *partition)
{
    char *end;
    if (strlen(partition) > 2) {
	end = partition + strlen(partition) - 1;
	if (*end == '/') *end = '\0';
    }
}

/**
 * create - Create a new non-replicated volume
 * @partitionpath:	Partition to create volume on
 * @volumeName:		Name of the new volume
 *
 * Create a non-replicated read-write volume named <volume-name> on partition
 * named <partition-path>. Non-replicated volumes are not cacheable by Coda
 * clients and are therefore not really useful. Use volutil create_rep to
 * create replicated volumes.
 */
static void create(void)
{
    char *partition, *volumeName;
    long volumeid = 0;

    if (these_args != 4) {
	fprintf(stderr, "Usage:  volutil create partition-path volumeName\n");
	exit(-1);
    }
    partition = this_argp[2];
    stripslash(partition);
    volumeName = this_argp[3];

    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName, (RPC2_Unsigned *)&volumeid, 0, 0);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %lx (%s) created \n", volumeid, volumeName);
    exit(0);
}

/**
 * clone - clone a volume replica
 * @volumeid:	Volume replica id
 * @-n_newvolumename:	Name of the clone of this volume.
 *
 * Create a read only clone of a read write volume with (replica) ID
 * (<volume-ID>). The vnodes are actually copied but the inodes are marked
 * copy-on-write i.e. inodes need to be copied only if modified. The name of
 * the new cloned volume can be optionally specified by the <new-volume-name>
 * parameter. Default value is volume-name.readonly. The clone(8) command can
 * be used to call volutil clone.
 */
static void clone(void)
{
    if (these_args != 3 && these_args != 5) {
	fprintf(stderr, "Usage: volutil clone <volume-id> [-n <new volume name>]\n");
	exit(-1);
    }
    long ovolid, newvolid;
    char buf[1];
    buf[0] = '\0';
    char *newvolname = buf;
    long rc;

    if (sscanf(this_argp[2], "%lX", &ovolid) != 1){
	fprintf(stderr, "Clone: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }
    if (these_args == 5){
	if (!strcmp(this_argp[3], "-n"))
	    newvolname = this_argp[4];
    }
    
    rc = VolClone(rpcid, ovolid, (RPC2_String)newvolname, (RPC2_Unsigned *)&newvolid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolClone failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolClone: New Volume id = %lx\n", newvolid);
    printf("VolClone: New Volume name is %s\n", newvolname);
    exit(0);
}

/**
 * dump - dump the volume contents
 * @-i_dumplevel: dump incremental
 * @volumeid:	volume replica id
 * @file:	file to dump into
 *
 * Dump the entire contents of a volume (volume-ID in Hex) to a file
 * (filename). If the -i flag is used, the dump will be incremental, it will
 * only include vnodes which have been modified since the last dump of a lower
 * incremental level was taken. The dump is not machine independent, certain
 * fields in the Vnode are not translated to network order. However, dump files
 * can be used to create restored volumes on machines with a similar
 * byte-order.
 */
static void dump(void)
{
    long rc = 0;
    RPC2_Unsigned Incremental = 0;
    int err = 0;
    FILE *outf;
    
    while ((these_args > 2) && *this_argp[2] == '-') {
	if (strcmp(this_argp[2], "-i") == 0) {
	    if (these_args > 4) {
	    	Incremental = (RPC2_Unsigned)atoi(this_argp[3]);
		these_args--; this_argp++;
	    } else
		Incremental = 1;
	}
	else
	    err = 1;

	these_args--; this_argp++;
    }
    if (err || these_args < 3) {
	fprintf(stderr, "Usage: volutil dump [-i [lvl]] <volume-id> [file]\n");
	exit(-1);
    }

    long volid;
    if (sscanf(this_argp[2], "%lX", &volid) != 1){
	fprintf(stderr, "Dump: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }

    if (these_args < 4) outf = stdout;
    else		outf = fopen(this_argp[3], "w");

    /* Create lwp thread DumpLwp(argv[3]) */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    rock->fd = fileno(outf);
    rock->volid = volid;
    rock->numbytes = 0;
    
    PROCESS dumpPid;
    LWP_CreateProcess((PFIC)VolDumpLWP, 16 * 1024, LWP_NORMAL_PRIORITY,
		      (char *)rock, "VolDumpLWP", &dumpPid);
    
    rc = VolNewDump(rpcid, volid, &Incremental);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "\nVolDump failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    fprintf(stderr, "\n%sVolDump completed, %ld bytes dumped\n",
	    Incremental ? "Incremental " : "", rock->numbytes);
    exit(0);
}

/**
 * dumpestimate - estimate the size of a volume dump
 * @volumeid:	volume replica id
 */
static void dumpestimate(void)
{
    long rc = 0;
    long volid;
    VolDumpEstimates sizes;
    
    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil dumpestimate <volume-id>\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &volid) != 1){
	fprintf(stderr, "Dump: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolDumpEstimate(rpcid, volid, &sizes);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolDumpEstimate failed with %s\n",
		RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    printf("Level0> %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu <Level9\n",
	   sizes.Lvl0, sizes.Lvl1, sizes.Lvl2, sizes.Lvl3, sizes.Lvl4,
	   sizes.Lvl5, sizes.Lvl6, sizes.Lvl7, sizes.Lvl8, sizes.Lvl9);
    fprintf(stderr, "VolDumpEstimate completed\n");

    exit(0);
}



static void VolDumpLWP(struct rockInfo *rock)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *myrequest;
    RPC2_Handle	mycid;
    register long rc;
    
    RPC2_SubsysIdent subsysid;

    /* Hide the dumpfile name under a rock for later retrieval. */
    CODA_ASSERT(LWP_NewRock(ROCKTAG, (char *)rock) == LWP_SUCCESS);
    
    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    
    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = VOLDUMP_SUBSYSTEMID;

    while (1) {
	rc=RPC2_GetRequest(&myfilter, &mycid, &myrequest, NULL, NULL, 0, NULL);
	if (rc == RPC2_SUCCESS) {
	    rc = volDump_ExecuteRequest(mycid, myrequest, NULL);
	    if (rc) {
		fprintf(stderr, "VolDumpLWP: request %ld failed with %s\n",
			myrequest->Header.Opcode, RPC2_ErrorMsg((int)rc));
	    }
	}
	else fprintf(stderr, "VolDumpLWP: Get_Request failed with %s\n",RPC2_ErrorMsg((int)rc));
    }

}

long WriteDump(RPC2_Handle rpcid, unsigned long offset, unsigned long *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    long rc = 0;
    struct rockInfo *rock;
    SE_Descriptor sed;
    
    CODA_ASSERT(LWP_GetRock(ROCKTAG, (char **)&rock) == LWP_SUCCESS);

    if (volid != rock->volid) {
	fprintf(stderr, "Got a WriteDump for %lx, I'm dumping %lx!\n",
		volid, rock->volid);
	exit(-1);
    }

    if (rock->numbytes != offset) {
	fprintf(stderr, "Offset %ld != rock->numbytes %ld\n",
		offset, rock->numbytes);
    }
    
    /* fetch the file with volume data */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sed.Value.SmartFTPD.ByteQuota = -1;
    sed.Value.SmartFTPD.SeekOffset = offset;
    sed.Value.SmartFTPD.hashmark = 0;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = rock->fd;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT){
	fprintf(stderr, "WriteDump: Error %s in InitSideEffect\n", RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) 
	       <= RPC2_ELIMIT) {
	fprintf(stderr, "WriteDump: Error %s in CheckSideEffect\n", RPC2_ErrorMsg((int)rc));
    }

    if (sed.Value.SmartFTPD.BytesTransferred != (int)*nbytes) {
	fprintf(stderr, "Transmitted bytes %ld != requested bytes %ld!\n",
	    *nbytes, sed.Value.SmartFTPD.BytesTransferred);
	*nbytes = sed.Value.SmartFTPD.BytesTransferred;
    }
#if 0
    fprintf(stderr, "Transmitted %ld bytes.\n",
	    sed.Value.SmartFTPD.BytesTransferred);
#else
    fprintf(stderr, ".");
#endif
    rock->numbytes += sed.Value.SmartFTPD.BytesTransferred;
    return rc;
}

/**
 *  restore - restore a volume dump
 *  @filename:	name of the dumpfile
 *  @partitionpath:	path to server partition
 *  @volname:	new volume name
 *  @volid:	new volume id
 *
 * Create a new volume on the partition named by <partition-path> and read in
 * the contents from a dump in file <file-name>. The new volume will be given
 * the name and ID specified on the command line. If either is unspecified, or
 * if the Volume ID is of illegal form, the server will allocate the ID or name
 * based on internal rules. The volume-ID should be specified in Hex.
 */
static void restorefromback(void)
{
    char volname[70], *partition;
    long rc = 0;
    long volid = 0;
    char *filename = NULL;
    FILE *outf;

    while ((these_args > 2) && *this_argp[2] == '-') {
	if (strcmp(this_argp[2], "-f") == 0) {
	    filename = this_argp[3];
	    these_args--; this_argp++;
	}

	these_args--; this_argp++;
    }

    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil restore [-f <file name>] <partition-name> [<volname> [<volid>]]\n");
	exit(-1);
    }

    partition = this_argp[2];
    stripslash(partition);

    if (these_args < 4) 
	memset((void *)volname, 0, 70);
    else
	strcpy(volname, this_argp[3]);

    if ((these_args < 5) || (sscanf(this_argp[4], "%lX", &volid) == 0))
	volid = 0;

    /* Create lwp thread DumpLwp */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    rock->volid = volid;
    rock->numbytes = 0;

    if (!filename) outf = stdin;
    else	   outf = fopen(filename, "r");

    rock->fd = fileno(outf);
    if (rock->fd < 0) {
	perror("RestoreFile");
	exit(-1);
    }
    
    PROCESS restorePid;
    LWP_CreateProcess((PFIC)VolDumpLWP, 16 * 1024, LWP_NORMAL_PRIORITY,
		      (char *)rock, "VolDumpLWP", &restorePid);
    if (rc != LWP_SUCCESS) {
	fprintf(stderr, "VolDump can't create child %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    rc = VolRestore(rpcid, (RPC2_String)partition, (RPC2_String)volname, (RPC2_Unsigned *)&volid);
    if (rc != RPC2_SUCCESS){
	fprintf(stderr, "\nVolRestore failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    printf("\nVolRestore successful, created %#8lx\n", volid);
    exit(0);
}

long ReadDump(RPC2_Handle rpcid, RPC2_Unsigned offset, RPC2_Integer *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    long rc = 0;
    struct rockInfo *rock;
    SE_Descriptor sed;
    char *buf;
    
    CODA_ASSERT(LWP_GetRock(ROCKTAG, (char **)&rock) == LWP_SUCCESS);

    if (volid == 0) { /* User didn't assign one, use volId fileserver gives us. */
	rock->volid = volid;
    }
    
    if (volid != rock->volid) {
	fprintf(stderr, "Got a ReadDump for %lx, I'm reading %lx!\n", volid, rock->volid);
	exit(-1);
    }

    /* Set up a buffer and read in the data from the dump file. */
    buf = (char *)malloc((unsigned int)*nbytes);
    if (!buf) {
	perror("ReadDump: Can't malloc buffer!");
	exit(-1);
    }

    CODA_ASSERT(rock->fd != 0);	/* Better have been opened by restore() */

    if (lseek(rock->fd, offset, L_SET) == -1) {
	perror("ReadDump: lseek");
	*nbytes = 0;
	free(buf);
	return 0;
    }

    *nbytes = read(rock->fd, buf, (int)*nbytes);
    if (*nbytes == -1) {
	perror("ReadDump: read");
	*nbytes = 0;
	free(buf);
	return 0;
    }
    
    /* fetch the file with volume data */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.ByteQuota = -1;
    sed.Value.SmartFTPD.SeekOffset = 0;
    sed.Value.SmartFTPD.hashmark = 0;
    sed.Value.SmartFTPD.Tag = FILEINVM;
    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqBody = (RPC2_ByteSeq)buf;
    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.MaxSeqLen = 
    sed.Value.SmartFTPD.FileInfo.ByAddr.vmfile.SeqLen = *nbytes;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT){
	fprintf(stderr, "ReadDump: Error %s in InitSideEffect\n", RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) 
	       <= RPC2_ELIMIT) {
	fprintf(stderr, "ReadDump: Error %s in CheckSideEffect\n", RPC2_ErrorMsg((int)rc));
    }

#if 0
    fprintf(stderr, "Transmitted %ld bytes.\n", sed.Value.SmartFTPD.BytesTransferred);
#else
    fprintf(stderr, ".");
#endif
    rock->numbytes += sed.Value.SmartFTPD.BytesTransferred;
    free(buf);
    return rc;
}

/**
 * dumpmem - dump a range of specified server memory
 * @address:	start address of dump
 * @size:	number of bytes to dump
 * @filename:	file to dump into
 *
 * Dump <size> bytes starting at <address> into <filename>.
 */
static void dumpmem(void) 
{
    if (these_args != 5) {
	fprintf(stderr, "Usage: volutil dumpmem <address> <size> <file-name>\n");
	exit(-1);
    }
    int address, size;
    char *fname;

    sscanf(this_argp[2], "%x", &address);
    sscanf(this_argp[3], "%d", &size);
    fname = this_argp[4];

    rc = VolDumpMem(rpcid, (RPC2_String)fname, address, size);
    if (rc != RPC2_SUCCESS){
	fprintf(stderr, "VolDumpMem failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Memory Dumped in file %s on server\n", fname);
    exit(0);
}


/**
 * rvmsize - display RVM statistics for a volume
 * @volumeid: volume replica id
 *
 * Print the RVM statistics for  the  volume  <volume-ID>.
 */
static void rvmsize(void)
{
    long volid;
    RVMSize_data data;
    
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil rvmsize <volid>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%lX", &volid) != 1){
	fprintf(stderr, "RVMSize: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolRVMSize(rpcid, volid, &data);
    if (rc != RPC2_SUCCESS){
	fprintf(stderr, "VolRVMSize failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %lx used a total of %ld bytes.\n", volid, data.VolumeSize);
    printf("\t%ld small vnodes used %ld bytes.\n", data.nSmallVnodes, data.SmallVnodeSize);
    printf("\t%ld large vnodes used %ld bytes.\n", data.nLargeVnodes, data.LargeVnodeSize);
    printf("\t and %ld bytes of DirPages.\n", data.DirPagesSize);
    exit(0);
}

/**
 * backup - create a backup clone
 * @volumeid:	volume replica to back up
 *
 * Create a backup clone of a read/write volume. If a backup clone already
 * exists, update it to reflect the current state of the read/write volume;
 * Otherwise, create a new read-only volume. The read/write volume must be
 * locked for this to succeed. Backup unlocks the volume as a side effect.
 */
static void backup(void)
{
    long Vid, backupVid;

    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil backup volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &Vid) != 1){
	fprintf(stderr, "VolMakeBackups: Bogus volume number %s\n",
		this_argp[2]);
	exit(-1);
    }

    rc = VolMakeBackups(rpcid, Vid, (VolumeId *)&backupVid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolMakeBackups failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Backup (id = %lx) of Volume %lx created\n", backupVid, Vid);
    exit(0);
}

/**
 * create_rep - create a replicated volume
 * @partitionpath:	server partition to create new volume on
 * @volumename:	name of the new volume
 * @groupid:	replicated group id of the volume
 * @rwvolid:	replica id of the new volume
 *
 * Create a replicated read-write volume named <volume-name> on partition named
 * <partition-path>. The <group-ID> parameter is used to specify the ID of the
 * replicated volume to which this replica will belong. The createvol_rep(8)
 * script provides a simple interface to create all necessary replicas on the
 * replica servers and updates the volume location and replication databases.
 */
static void create_rep(void)
{
    char *partition, *volumeName;
    long volumeid = 0;
    long groupid;

    if (these_args != 5 && these_args != 6) {
	fprintf(stderr, "Usage:  volutil create_rep partition-path volumeName grpid [rw-volid]\n");
	exit(-1);
    }
    partition = this_argp[2];
    stripslash(partition);
    volumeName = this_argp[3];

    if (sscanf(this_argp[4], "%lX", &groupid) != 1){
	fprintf(stderr, "CreateRep: Bad Group Id %s\n", this_argp[4]);
	exit(-1);
    }
    if (these_args == 6) {
	if (sscanf(this_argp[5], "%lX", &volumeid) != 1){
	    fprintf(stderr, "CreateRep: Bad Volume Id %s\n", this_argp[5]);
	    exit(-1);
	}
    }

    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName,
		   (RPC2_Unsigned *)&volumeid, 1, groupid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %lx (%s) created \n", volumeid, volumeName);
    exit(0);
}


/**
 * makevldb - build a new volume location database
 * @volumelist - concatenation of the VolumeList file from all servers
 *
 * Create a new Volume Location Database VLDB. VolumeList names a file
 * containing volume parameters for all volumes in the system. This command
 * typically is run on the system control machine. See also bldvldb(8) and
 * volumelist(5).
 */
static void makevldb(void)
{
    char *infile;
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil makevldb VolumeListFile\n");
	exit (-1);
    }
    infile = this_argp[2];

    rc = VolMakeVLDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolMakeVLDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "VLDB completed.\n");
    exit(0);
}

/**
 * makevrdb - create a new volume replication database
 * @vrlist:	volume replication list which contains information about all
 *		volume replicas
 *
 * Create a new Volume Replication Data Base (VRDB). <vrlist> is a file
 * containing entries describing replicated volumes. Each entry contains the
 * name, group-ID, read-write ids, and the VSG address of a replicated volume.
 * There is one entry per replicated volume in the system.
 */
static void makevrdb(void)
{
    char *infile;
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil makevrdb VRListFile\n");
	exit (-1);
    }
    infile = this_argp[2];

    rc = VolMakeVRDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolMakeVRDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "VRDB completed.\n");
    exit(0);
}

/**
 * info - get detailed volume information
 * @-all:	also get contents of large and small vnodes
 * @volumename/id:	name or id of the volume
 * @file:	write information to this file
 *
 * Print in ascii the contents of a volume to stdout (or the file as specified
 * by -o). The volume can be specified by its name, or by the volume-ID,
 * specified in Hex. If -all is specified, contents of both large and small
 * vnodes in that volume are also printed. 
 */
static void info(void)
{
    long DumpAll = 0;
    SE_Descriptor sed;
    int err = 0;
    FILE *outf;

    while ((these_args > 2) && *this_argp[2] == '-') {
	if (strcmp(this_argp[2], "-all") == 0) {
	    DumpAll = 1;
	}
	else {
	    err = 1;
	}
	these_args--;
	this_argp++;
    }
    if (err || these_args < 3) {
	fprintf(stderr, "Usage: volutil info [-all] volumeName/volumeNumber [file]\n");
	exit(-1);
    }

    if (these_args < 4) outf = stdout;
    else		outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = VolInfo(rpcid, (RPC2_String)this_argp[2], DumpAll, &sed);
    if (rc == -1) {
	fprintf(stderr, "VolInfo failed, %s not found\n", this_argp[2]);
	exit(-1);
    }
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolInfo failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    exit(0);
}

/*
  BEGIN_HTML
  <a name="showvnode"><strong>Client end of the <tt>showvnode</tt> request</strong></a>
  END_HTML
*/
static void showvnode(void)
{
    SE_Descriptor sed;
    bit32 volumeNumber, vnodeNumber;
    bit32 unique;
    FILE *outf;

    if (these_args < 5) {
	fprintf(stderr, "Usage: volutil showvnode volumeNumber vnodeNumber Unique [file]\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &volumeNumber) != 1){
	fprintf(stderr, "showvnode: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    if (sscanf(this_argp[3], "%lX", &vnodeNumber) != 1){
	fprintf(stderr, "showvnode: Bogus vnode number %s\n", this_argp[3]);
	exit(-1);
    }
    if (sscanf(this_argp[4], "%lX", &unique) != 1) {
	fprintf(stderr, "showvnode: Bogus Uniquifier %s\n", this_argp[4]);
	exit(-1);
    }

    if (these_args < 6) outf = stdout;
    else		outf = fopen(this_argp[5], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = VolShowVnode(rpcid, volumeNumber, vnodeNumber, unique, &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolShowVnode failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    exit(0);
}
/*
  BEGIN_HTML
  <a name="setvv"><strong>Client end of the <tt>setvv</tt> request</strong></a> 
  END_HTML
*/
static void setvv(void) 
{
    bit32   volumeNumber, vnodeNumber;
    bit32   unique;
    ViceVersionVector	vv;

    if (these_args != 16){
	fprintf(stderr, "Usage: volutil setvv volumeNumber vnodeNumber unique <version nubmers(8)> <StoreId (host) (Uniquifier)> <flags>\n");
	exit(-1);
    }
    
    if (sscanf(this_argp[2], "%lX", &volumeNumber) != 1){
	fprintf(stderr, "setvv: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    if (sscanf(this_argp[3], "%lX", &vnodeNumber) != 1){
	fprintf(stderr, "setvv: Bogus vnode number %s\n", this_argp[3]);
	exit(-1);
    }
    if (sscanf(this_argp[4], "%lX", &unique) != 1) {
	fprintf(stderr, "setvv: Bogus vnode uniquifier %s\n", this_argp[4]);
	exit(-1);
    }
    memset((void *)&vv, 0, sizeof(vv));
    vv.Versions.Site0 = (bit32) atoi(this_argp[5]);
    vv.Versions.Site1 = (bit32) atoi(this_argp[6]);
    vv.Versions.Site2 = (bit32) atoi(this_argp[7]);
    vv.Versions.Site3 = (bit32) atoi(this_argp[8]);
    vv.Versions.Site4 = (bit32) atoi(this_argp[9]);
    vv.Versions.Site5 = (bit32) atoi(this_argp[10]);
    vv.Versions.Site6 = (bit32) atoi(this_argp[11]);
    vv.Versions.Site7 = (bit32) atoi(this_argp[12]);
    
    vv.StoreId.Host = atol(this_argp[13]);
    vv.StoreId.Uniquifier = atol(this_argp[14]);
    vv.Flags = atol(this_argp[15]);
    
    rc = VolSetVV(rpcid, volumeNumber, vnodeNumber, unique, &vv);
    if (rc != RPC2_SUCCESS){
	fprintf(stderr, "VolSetVV failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "VolSetVV completed\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="purge"><strong>Client end of the <tt>purge</tt> request</strong></a> 
  END_HTML
*/
static void purge(void)
{
    long rc;
    VolumeId volid = 0;

    if (these_args != 4) {
	fprintf(stderr, "Usage: volutil purge VolumeId VolumeName\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &volid) != 1){
	fprintf(stderr, "Purge: Bad Volume Id %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolPurge(rpcid, volid, (RPC2_String)this_argp[3]);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolPurge failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %lx (%s) successfully purged\n", volid, this_argp[3]);
    exit(0);
}
/*
  BEGIN_HTML
  <a name="lock"><strong>Client end of request to <tt>lock</tt> a volume</strong></a>
  END_HTML
*/
static void lock(void)
{
    long Vid;
    ViceVersionVector	vvv;
    long rc;
    
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil lock volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &Vid) != 1){
	fprintf(stderr, "VolLock: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolLock(rpcid, Vid, &vvv);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolLock failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Locked volume %lx had a VVV of (%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld)\n",
	   Vid, vvv.Versions.Site0, vvv.Versions.Site1, vvv.Versions.Site2,
	   vvv.Versions.Site3, vvv.Versions.Site4, vvv.Versions.Site5,
	   vvv.Versions.Site6, vvv.Versions.Site7);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="unlock"><strong>Client end of request to <tt>unlock</tt> a volume</strong></a>
  END_HTML
*/
static void unlock(void)
{
    long Vid;
    
    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil unlock volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &Vid) != 1){
	fprintf(stderr, "VolUnlock: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolUnlock(rpcid, Vid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolUnlock failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %lx is unlocked.\n", Vid);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="lookup"><strong>Client end of request to <tt>lookup</tt> a volume</strong></a>
  END_HTML
*/
static void lookup(void)
{
    SE_Descriptor sed;
    FILE *outf;

    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil lookup <volume name or id> [file]\n");
	exit(-1);
    }

    if (these_args < 4) outf = stdout;
    else		outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = VolLookup(rpcid, (RPC2_String)this_argp[2], &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolLookup failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    exit(0);
    
}
/*
  BEGIN_HTML
  <a name="updatedb"><strong>Client end of the <tt>updatedb()</tt> request</strong> </a>
  END_HTML
 */
static void updatedb(void)
{
    if (these_args != 2) {
	fprintf(stderr, "Usage: volutil updatedb\n");
	exit (-1);
    }

    rc = VolUpdateDB(rpcid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolUpdateDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Databases updated on host %s.\n", s_hostname);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="shutdown"><strong>Client end of the <tt>shutdown</tt> request</strong></a> 
  END_HTML
*/
static void shutdown(void)
{
    if (these_args != 2) {
	fprintf(stderr, "Usage: volutil shutdown\n");
	exit (-1);
    }

    rc = VolShutdown(rpcid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolShutdown failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Fileserver shutdown.\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="swaplog"><strong>Client end of the <tt>swaplog</tt> request</strong></a> 
  END_HTML
*/
static void swaplog(void)
{
    if (these_args != 2) {
	fprintf(stderr, "Usage: volutil swaplog\n");
	exit (-1);
    }
    
    rc = VolSwaplog(rpcid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSwaplog failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Fileserver log successfully swapped.\n");
    exit(0);
}


/*
  BEGIN_HTML
  <a name="swapmalloc"><strong>Client end of the <tt>togglemalloc</tt> request</strong></a> 
  END_HTML
*/
static void swapmalloc(void)
{
    if (these_args != 2) {
	fprintf(stderr, "Usage: volutil togglemalloc\n");
	exit (-1);
    }
    
    rc = VolSwapmalloc(rpcid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSwapmalloc failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Malloc tracing successfuly toggled.\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="setdebug"><strong>Client end of the <tt>setdebug()</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void setdebug(void)
{
    int debuglevel = 0;

    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil setdebug debuglevel\n");
	exit (-1);
    }
    debuglevel = atoi(this_argp[2]);

    rc = VolSetDebug(rpcid, debuglevel);

    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSetDebug failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "VolumeDebugLevel set to %d.\n", debuglevel);
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="timing"><strong>Client end of the <tt>timing</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void timing(void)
{
    int on = -1;
    SE_Descriptor sed;
    FILE *outf;

    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil timing <on | off> [file]\n");
	exit(-1);
    }
    
    if (strcmp(this_argp[2], "on") == 0)
	on = 1;
    else if (strcmp(this_argp[2], "off") == 0)
	on = 0;

    if (on == -1) { 
	fprintf(stderr, "Usage: volutil timing <on | off>\n");
	exit(-1);
    }

    if (these_args < 4) outf = stdout;
    else		outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = VolTiming(rpcid, on, &sed);
    
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolTiming failed with return code %ld\n", rc);
	exit(-1);
    }
    fprintf(stderr, "Timing finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="tracerpc"><strong>Client end of the <tt>tracerpc</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void tracerpc(void)
{
    SE_Descriptor sed;
    FILE *outf;

    if (these_args < 3) {
	fprintf(stderr, "Usage: volutil tracerpc [outfile <file>]\n");
	exit(-1);
    }

    if (these_args < 4) outf = stdout;
    else		outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = TraceRpc(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "TraceRpc failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    fprintf(stderr, "TraceRpc finished successfully\n");
    exit(0);
}

/**
 * printstats - get server statistics.
 * @file: optional output file
 *
 */
static void printstats(void)
{
    SE_Descriptor sed;
    FILE *outf;

    if (these_args < 2) {
	fprintf(stderr, "Usage: volutil printstats [<file>]\n");
	exit(-1);
    }

    if (these_args < 3) outf = stdout;
    else		outf = fopen(this_argp[2], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = PrintStats(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "PrintStats failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "PrintStats finished successfully\n");
    exit(0);
}

/**
 * getvolumelist - get VolumeList from the server
 * @file: optional output file
 *
 */
static void getvolumelist(void)
{
    SE_Descriptor sed;
    FILE *outf;

    if (these_args < 2) {
	fprintf(stderr, "Usage: volutil getvolumelist [<file>]\n");
	exit(-1);
    }

    if (these_args < 3) outf = stdout;
    else		outf = fopen(this_argp[2], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = GetVolumeList(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "GetVolumeList failed with %s\n",
		RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "GetVolumeList finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="showcallbacks"><strong>Client end of the <tt>showcallbacks</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void showcallbacks(void)
{
    ViceFid fid;
    FILE *outf;

    if (these_args < 5) {
	fprintf(stderr, "Usage: volutil showcallbacks <volumeid> <vnode> <unique> [file]\n");
	exit(-1);
    }
    
    if ((sscanf(this_argp[2], "%lX", &fid.Volume) != 1) ||
	(sscanf(this_argp[3], "%lX", &fid.Vnode) != 1) ||
	(sscanf(this_argp[4], "%lX", &fid.Unique) != 1)) {
	fprintf(stderr, "Usage: volutil showcallbacks <volumeid> <vnode> <unique> <out-file>\n");
	exit(-1);
    }

    if (these_args < 6) outf = stdout;
    else		outf = fopen(this_argp[5], "w");

    /* set up side effect descriptor */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = fileno(outf);

    rc = ShowCallbacks(rpcid, &fid, &sed);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "Showcallbacks failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    fprintf(stderr, "Showcallbacks finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="truncatervmlog"><strong>Client end of the <tt>truncatervmlog</tt> request</strong></a> 
  END_HTML
*/
static void truncatervmlog(void)
{
    rc = TruncateRVMLog(rpcid);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "Couldn't truncate log\n");
	exit(-1);
    }
    fprintf(stderr, "Truncate of RVM log started....Wait for a few minutes for it to complete\n");
    exit(0);
}


/*
  BEGIN_HTML
  <a name="getmaxvol"><strong>Client end of the getmaxvol request</strong></a> 
  END_HTML
*/
static void getmaxvol(void)
{
    VolumeId maxid;

    if (these_args != 2) {
	fprintf(stderr, "Usage: volutil getmaxvol\n");
	exit(-1);
    }

    rc = VolGetMaxVolId(rpcid, (RPC2_Integer *)&maxid);
    if (rc != RPC2_SUCCESS) { 
        fprintf(stderr, "Couldn't get maxvolid: %s\n", RPC2_ErrorMsg((int)rc));
	exit (-1);
    }
    printf("Maximum volume id is 0x%lX\n", maxid);
    exit (0);
}


/*
  BEGIN_HTML
  <a name="setmaxvol"><strong>Client end of the setmaxvol request</strong></a> 
  END_HTML
*/
static void setmaxvol(void)
{
    VolumeId volid;

    if ((these_args != 3) || (sscanf(this_argp[2], "%lX", &volid) != 1)) {
        fprintf(stderr, "Usage: volutil setmaxvol <volumeid>\n");
	exit(-1);
    }

    rc = VolSetMaxVolId(rpcid, volid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr,
"Couldn't set new maximum volume id, check that you didn't try\n"
"to change the server id or set a new maxid that is less than\n"
"the current maximum id.\n");
	exit(-1);
    }

    printf("Maximum volume id set to 0x%lx\n", volid);
    exit(0);
}

static void peekpokeerr(void)
{
    static	char	*msgs[]={
	/*0*/ "Path to server file not known",
	/*1*/ "Cannot read symbols from the server file",
	/*2*/ "Symbol not found in the server file",
	/*3*/ "Address not in server virtual space",
	/*4*/ "Memory protection error",
	/*5*/ "Unaligned integer/pointer reference"};
    fprintf(stderr, "Couldn't %s at %s: %s\n", this_argp[1], this_argp[2],
	    (int)rc >= -10040L && rc < (int)(sizeof(msgs)/sizeof(*msgs)-10040L) ?
	    msgs[10040 + (int) rc] : RPC2_ErrorMsg((int) rc));
    exit (-1);
}

static void usageerr(char *args)
{
    fprintf(stderr, "Usage: %s %s %s\n", this_argp[0], this_argp[1], args);
    exit(-1);
}

static int sscani(char *s, RPC2_Integer *px)
{
    long k;
    int	q;

    while(isspace(*s)) s++;

    if (*s != '0')	  q = sscanf(s, "%ld", &k);
    else if (*++s != 'x') q = sscanf(s, "%lo", &k);
    else		  q = sscanf(++s, "%lx", &k);

    *px = (RPC2_Integer) k;
    return(q);
}

/*
  BEGIN_HTML
  <a name="peekint"><strong>Client end of the peek request</strong></a> 
  END_HTML
*/
static void peekint(void)
{
    RPC2_Integer value;

    if (these_args != 3) usageerr("<address>");

    if ((rc = VolPeekInt(rpcid, (RPC2_String) this_argp[2], &value)) != RPC2_SUCCESS)
	peekpokeerr();
    printf("%s contains 0x%lx\n", this_argp[2], (long) value);
    exit (0);
}


/*
  BEGIN_HTML
  <a name="pokeint"><strong>Client end of the poke request</strong></a> 
  END_HTML
*/
static void pokeint(void)
{
    RPC2_Integer value;

    if ((these_args != 4) || (sscani(this_argp[3], &value) != 1))
	usageerr("<address> <value>");

    if ((rc = VolPokeInt(rpcid, (RPC2_String) this_argp[2], value)) != RPC2_SUCCESS)
	peekpokeerr();
    fprintf(stderr, "0x%lx stored at %s\n", (long) value, this_argp[2]);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="peekmem"><strong>Client end of the <tt>peeks()</tt> request</strong></a> 
  END_HTML
*/
static void peekmem(void)
{
    RPC2_BoundedBS buf;

    if ((these_args != 4) || (sscani(this_argp[3], &buf.MaxSeqLen) != 1))
	usageerr("<address> <size>");

    if ((buf.SeqBody = (RPC2_String) malloc((int) buf.MaxSeqLen + 1)) == NULL) {
	fprintf(stderr, "volutil: Out of memory\n");
	exit(-1);
    }
    buf.SeqLen = 0;

    if ((rc = VolPeekMem(rpcid, (RPC2_String) this_argp[2], &buf)) != RPC2_SUCCESS)
	peekpokeerr();
    buf.SeqBody[(int) buf.SeqLen] = '\0';
    printf("%s contains %s\n", this_argp[2], buf.SeqBody);
    exit (0);
}


/*
  BEGIN_HTML
  <a name="pokemem"><strong>Client end of the <tt>pokes()</tt> request</strong></a> 
  END_HTML
*/
static void pokemem(void)
{
    RPC2_CountedBS buf;

    if ((these_args != 5) || (sscani(this_argp[3], &buf.SeqLen) != 1))
	usageerr("<address> <size> <value>");

    buf.SeqBody = (RPC2_String) this_argp[4];

    if ((rc = VolPokeMem(rpcid, (RPC2_String) this_argp[2], &buf)) != RPC2_SUCCESS)
	peekpokeerr();
    printf("%s stored at %s\n", buf.SeqBody, this_argp[2]);

    exit(0);
}


/*
  BEGIN_HTML
  <a name="peekxmem"><strong>Client end of the <tt>peekx()</tt> request</strong></a> 
  END_HTML
*/
static void peekxmem(void)
{
    RPC2_BoundedBS buf;

    if ((these_args != 4) || (sscani(this_argp[3], &buf.MaxSeqLen) != 1))
	usageerr("<address> <size>");

    if ((buf.SeqBody = (RPC2_String) malloc((int) buf.MaxSeqLen)) == NULL) {
	fprintf(stderr, "volutil: Out of memory\n");
	exit(-1);
    }
    buf.SeqLen = 0;

    if ((rc = VolPeekMem(rpcid, (RPC2_String) this_argp[2], &buf)) != RPC2_SUCCESS)
	peekpokeerr();
    printf("%s contains 0x", this_argp[2]);
    while(buf.SeqLen--) printf("%02x", *buf.SeqBody++);
    printf("\n");
    exit (0);
}

/*
  BEGIN_HTML
  <a name="pokexmem"><strong>Client end of the <tt>pokex()</tt> request</strong></a> 
  END_HTML
*/
static void pokexmem(void)
{
    RPC2_CountedBS buf;
    char	*t, *s;
    RPC2_Integer size;

    if ((these_args != 5) || (sscani(this_argp[3], &buf.SeqLen) != 1))
	usageerr("<address> <size> <hexvalue>");

    if ((buf.SeqBody = (RPC2_String) malloc((int) buf.SeqLen)) == NULL) {
	fprintf(stderr, "volutil: Out of memory\n");
	exit(-1);
    }
    if ((s = this_argp[4])[0] == '0' && s[1] == 'x') s += 2;
    t = (char *) buf.SeqBody;
    size = buf.SeqLen;
    while(size--) {
	int	vh, vl;
	if (!isxdigit(s[0]) || !isxdigit(s[1])) {
	    fprintf(stderr, "%s is not a %s-byte hex string\n",
		    this_argp[4], this_argp[3]);
	    usageerr("<address> <size> <hexvalue>");
	}
	vh = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
	s++ ;
	vl = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
	s++ ;
	*t++ = (vh << 4) + vl;
    }

    if ((rc = VolPokeMem(rpcid, (RPC2_String) this_argp[2], &buf)) != RPC2_SUCCESS)
	peekpokeerr();
    printf("0x%s stored at %s\n",
	    (this_argp[4][0] == '0' && this_argp[4][1] == 'x') ?
	    this_argp[4] + 2 : this_argp[4],
	    this_argp[2]);
    exit(0);
}

static void setwb(RPC2_Integer wbflag)
{
    long Vid;

    if (these_args != 3) {
	fprintf(stderr, "Usage: volutil [en,dis]ablewb volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%lX", &Vid) != 1){
	fprintf(stderr, "VolSetWBParms: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }

    rc = VolSetWBParms(rpcid, Vid, wbflag);
    if (rc != RPC2_SUCCESS) {
	fprintf(stderr, "VolSetWBParms failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("WriteBack %sallowed for Volume %lx\n", wbflag ? "" : "dis", Vid);

    exit(0);
}

static void V_InitRPC(int timeout)
{
    PROCESS mylpid;
    FILE *tokfile;
    SFTP_Initializer sftpi;
    struct timeval tout;
    long rcode;

    /* store authentication key */
    tokfile = fopen(vice_sharedfile(VolTKFile), "r");
    if (!tokfile) {
	char estring[80];
	sprintf(estring, "Tokenfile %s", vice_sharedfile(VolTKFile));
	perror(estring);
	exit(-1);
    }
    memset(vkey, 0, RPC2_KEYSIZE);
    fread(vkey, 1, RPC2_KEYSIZE, tokfile);
    fclose(tokfile);

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi);
    tout.tv_sec = timeout;
    tout.tv_usec = 0;
    rcode = RPC2_Init(RPC2_VERSION, 0, NULL, 3, &tout);
    if (rcode != RPC2_SUCCESS) {
	fprintf(stderr, "RPC2_Init failed with %s\n", RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}


static int V_BindToServer(char *fileserver, RPC2_Handle *RPCid)
{
 /* Binds to File Server on volume utility port on behalf of uName.
    Sets RPCid to the value of the connection id.    */

    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_EncryptionKey secret;
    long     rcode;

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, fileserver);
#ifdef __CYGWIN32__
	/* XXX -JJK */
	pident.Tag = RPC2_PORTBYINETNUMBER;
	pident.Value.InetPortNumber = htons(PORT_codasrv);
#else
    pident.Tag = RPC2_PORTBYNAME;
    strcpy(pident.Value.Name, "codasrv");
#endif
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = UTIL_SUBSYSID;

    fprintf(stderr, "V_BindToServer: binding to host %s\n", fileserver);
    RPC2_BindParms bparms;
    memset((void *)&bparms, 0, sizeof(bparms));
    bparms.SecurityLevel = RPC2_AUTHONLY;
    bparms.EncryptionType = RPC2_XOR;
    bparms.SideEffectType = SMARTFTP;

    GetSecret(vice_sharedfile(VolTKFile), secret);
    bparms.SharedSecret = &secret;

    rcode = RPC2_NewBinding(&hident, &pident, &sident, &bparms, RPCid);
    if (rcode < 0 && rcode > RPC2_ELIMIT)
	rcode = 0;
    if (rcode == 0 || rcode == RPC2_NOTAUTHENTICATED)
	return(rcode);
    else {
	fprintf(stderr, "RPC2_NewBinding to server %s failed with %s\n",
				fileserver, RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}

