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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/volclient.cc,v 4.12 1998/10/05 15:33:39 braam Exp $";
#endif /*_BLURB_*/








/***************************************************/
/*	volclient.c				   */
/*	    - client side for volume utilities	   */
/***************************************************/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

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

#include <lwp.h>
#include <lock.h>
#include <timer.h>
#include <rpc2.h>
#include <se.h>
#include <sftp.h>
#include <util.h>
#include <partition.h>
#include <ports.h>
#include <vice.h>
#include <callback.h>
#include <volutil.h>
#include <voldump.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>
#include "velapse.h"

static char vkey[RPC2_KEYSIZE+1];	/* Encryption key for bind authentication */
/* file variables for utils with file transfer */
static char infile[256];
static char outfile[256];	// file name for requested info
/* hack to make argc and argv visible to subroutines */
static char **this_argp;
static int these_args;

int VolumeChanged = 0;	/* needed by physio.c, not really used */

static char s_hostname[100];
static int timeout = 0;
static RPC2_Handle rpcid;
static long rc;

static void backup();
static void salvage();
static void create();
static void create_rep();
static void clone();
static void makevldb();
static void makevrdb();
static void info();
static void showvnode();
static void setvv();
static void purge();
static void lookup();
static void lock();
static void unlock();
static void updatedb();
static void shutdown();
static void swaplog();
static void swapmalloc();
static void setdebug();
static void oldstyledump();
static void dump();
static void restorefromback();
static void dumpmem();
static void rvmsize();
static void setlogparms();
static void markasancient();
void timing();
static void elapse();
static void tracerpc();
static void printstats();
static void showcallbacks();
static void truncatervmlog();
static void getmaxvol();
static void setmaxvol();
static void peekint();
static void pokeint();
static void peekmem();
static void pokemem();
static void peekxmem();
static void pokexmem();

#define ROCKTAG 12345
struct rockInfo {
    char dumpfile[128];	    /* should be enough to hold a reasonable pathname. */
    int fd;		    /* Open filedescriptor for ReadDump. */
    VolumeId volid;	    /* Volume being dumped. */
    unsigned long numbytes; /* Number of bytes already written to the file. */
};

static void V_InitRPC(int timeout);
static int V_BindToServer(char *fileserver, RPC2_Handle *RPCid);
static void Die(char *msg);
static void VolDumpLWP(struct rockInfo *rock);
extern long volDump_ExecuteRequest(RPC2_Handle, RPC2_PacketBuffer*,SE_Descriptor*);

int main(int argc, char **argv) {
#ifndef __CYGWIN32__
	/* XXX -JJK */
    if (getuid() != 0) {
	printf("Volume utilities must be run as root; sorry\n");
	exit(1);
    }
#endif

    if (argc < 2) {
        printf("Usage: volutil [-h hostname] [-t timeout] [-d debuglevel]  <option>, where <option> is one of the following:\n");
	printf("ancient, backup, create, create_rep, clone, dump, info, lock, ");
	printf("lookup, makevldb, makevrdb, purge, restore, salvage, ");
	printf("setvv, showvnode, shutdown, swaplog, setdebug, updatedb, ");
	printf("unlock, dumpmem, rvmsize, timing, ");
	printf("elapse, truncatervmlog, togglemalloc, ");
	printf("getmaxvol, setmaxvol, peek, poke, peeks, pokes, peekx, pokex\n");
	exit(-1);
    }

    /* Set the default timeout and server host */
    timeout = 30;	/* Default rpc2 timeout is 30 seconds. */
    gethostname(s_hostname, sizeof(s_hostname) -1);
	
    while (argc > 2 && *argv[1] == '-') { /* Both options require 2 arguments. */
	if (strcmp(argv[1], "-h") == 0) { /* User specified other host. */
	    struct hostent *hp;
	    argv++; argc--;
	    hp = gethostbyname(argv[1]);
	    if (hp) {
		strcpy(s_hostname, hp->h_name);
		argv++; argc--;
	    } else {
		printf("%s is not a valid host name.\n", argv[1]);
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
	    /* User gave timeout */
	    RPC2_DebugLevel = atoi(argv[2]);
	    VolDebugLevel = atoi(argv[2]);
	    argv++; argc--;
	    argv++; argc--;
	}
    }
    
    CODA_ASSERT(s_hostname != NULL);
    V_InitRPC(timeout);
    V_BindToServer(s_hostname, &rpcid);

    if (argc < 2) {
        printf("Usage: volutil [-h hostname]  [-t timeout] <option>, where <option> is one of the following:\n");
	printf("ancient, backup, create, create_rep, clone, dump, info, lock, ");
	printf("lookup, makevldb, makevrdb, purge, restore, salvage, ");
	printf("setvv, showvnode, shutdown, swaplog, setdebug, updatedb, ");
	printf("unlock, dumpmem, rvmsize, timing, elapse, \n");
	printf("printstatus, showcallbacks, truncatervmlog, togglemalloc, ");
	printf("getmaxvol, setmaxvol, peek, poke, peeks, pokes, peekx, pokex\n");
	exit(-1);
    }

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
    else if (strcmp(argv[1], "elapse") == 0)
	elapse();
    else if (strcmp(argv[1], "tracerpc") == 0)
	tracerpc();
    else if (strcmp(argv[1], "printstats") == 0)
	printstats();
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
/*    else if (strcmp(argv[1], "checkrec") == 0)
 *	checkrec();
 *    else if (strcmp(argv[1], "dumprecstore") == 0)
 *	dumprecstore();
 */
    else {
	printf("Bad vol function. Options are:\n");
	printf("ancient, backup, create, create_rep, clone, dump, info, lock, ");
	printf("lookup, makevldb, makevrdb, purge, restore, salvage, ");
	printf("setvv, showvnode, shutdown, swaplog, setdebug, updatedb, ");
	printf("unlock, dumpmem, rvmsize, setlogparms, tracerpc, printstats, ");
	printf("showcallbacks, truncatervmlog, getmaxvol, setmaxvol, ");
	printf("peek, poke, peeks, pokes, peekx, togglemalloc, pokex\n");
	exit(-1);
    }
    return 0;
}

/*
  BEGIN_HTML
  <a name="ancient"><strong>Client end of the <tt>ancient</tt> request</strong></a> 
  END_HTML
*/
static void markasancient() 
{
    long repid, groupid;
    
    if (these_args != 4) {
	printf("Usage: volutil ancient <groupid> <repid>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%X", &groupid) != 1){
	printf("MarkAsAncient: Bad Groupid %s\n", this_argp[2]);
	exit(-1);
    }
    if (sscanf(this_argp[3], "%X", &repid) != 1){
	printf("MarkAsAncient: Bad Repid %s\n", this_argp[3]);
	exit(-1);
    }

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-ancient.c.html#S_VolMarkAsAncient">Server implementation of VolMarkAsAncient()</a></pre>
      END_HTML
    */
    rc = VolMarkAsAncient(rpcid, groupid, repid);
    if (rc != RPC2_SUCCESS){
	printf("VolMarkAsAncient failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    exit(0);	/* Funny, need to exit or the program never exits... */
}

/*
  BEGIN_HTML
  <a name="setlogparms"><strong>Client end of the <tt>setlogparms</tt> request</strong></a> 
  END_HTML
*/
static void setlogparms() {
    long volid;
    long flag;
    long nentries;
    int i;

    nentries = 0;
    flag =  -1;
    
    if (these_args < 5) {
	printf("Usage: volutil setlogparms <volid> reson <flag> logsize <nentries>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%X", &volid) != 1) {
	printf("setlogparms: Bad VolumeId %s\n", this_argp[2]);
	exit(-1);
    }
    for (i = 3; i < these_args ; i++) {
	if (strcmp(this_argp[i], "reson") == 0) {
	    i = i + 1;
	    if (sscanf(this_argp[i], "%d", &flag) != 1) {
		printf("Bad flag value %s\n", this_argp[i]);
		exit(-1);
	    }
	}
	if (strcmp(this_argp[i], "logsize") == 0) {
	    i = i + 1;
	    if (sscanf(this_argp[i], "%d", &nentries) != 1) {
		printf("Bad logsize value %s\n", this_argp[i]);
		exit(-1);
	    }
	}
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-setlogparms.c.html#S_VolSetLogParms">Server implementation of VolSetLogParms()</a></pre>
      END_HTML
    */
    
    rc = VolSetLogParms(rpcid, volid, flag, nentries);
    if (rc != RPC2_SUCCESS) {
	printf("VolSetLogParms failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Set Log parameters\n");
    exit(0);
}
static void salvage() {
    int err = 0;
    int debug = 0;			/* -d flag */
    int listinodeoption = 0;
    int forcesalvage = 0;
    VolumeId vid = 0;
    char *path = NULL;

    if (these_args < 3) {
	printf("Usage: volutil salvage [-d][-f][-i] partition [rw-vol number]\n");
	exit(-1);
    }
    these_args--; this_argp++;
    these_args--; this_argp++;
    while (these_args && **this_argp == '-') {
	if (strcmp(*this_argp,"-d") == 0)
	    debug = 1;
	else if (strcmp(*this_argp,"-t") == 0) {
	    printf("Testing option not implemented\n");
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
	printf("Usage: volutil salvage [-d] [-f] [-i] partition [read/write-volume-number]\n");
	exit(-1);
    }
    if (these_args > 0)
	path = this_argp[0];
    if (these_args == 2) {
	if (sscanf(this_argp[1], "%x", &vid) != 1){
	    printf("salvage: invalid volume id specified; salvage aborted\n");
	    exit(1);
	}
    }

    rc = VolSalvage (rpcid, (RPC2_String)path, vid, forcesalvage, debug, listinodeoption);
    if (rc != RPC2_SUCCESS) {
	printf("VolSalvage failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Salvage complete.\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="create"><strong>Client end of the <tt>create</tt> request</strong></a> 
  END_HTML
*/
static void create() {
    char *partition, *volumeName;
    long volumeid = 0;

    if (these_args != 4) {
	printf("Usage:  volutil create partition-path volumeName\n");
	exit(-1);
    }
    partition = this_argp[2];
    volumeName = this_argp[3];
#if 0 /* XXX this needs to go to the server end now */
    if (strncmp(partition, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE) != 0) {
	printf("Invalid partition specified, %s\n", (int)partition);
	exit(1);
    }
#endif
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-create.c.html#S_VolCreate">Server implementation of VolCreate()</a></pre>
      END_HTML
    */
    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName, (RPC2_Unsigned *)&volumeid, 0, 0);
    if (rc != RPC2_SUCCESS) {
	printf("VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %x (%s) created \n", volumeid, volumeName);
    exit(0);
}
/*
clone: Client end of the clone request
*/
static void clone()
{
    if (these_args != 3 && these_args != 5) {
	printf("Usage: volutil clone <volume-id> [-n <new volume name>]\n");
	exit(-1);
    }
    long ovolid, newvolid;
    char buf[1];
    buf[0] = '\0';
    char *newvolname = buf;
    long rc;

    if (sscanf(this_argp[2], "%X", &ovolid) != 1){
	printf("Clone: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }
    if (these_args == 5){
	if (!strcmp(this_argp[3], "-n"))
	    newvolname = this_argp[4];
    }
    
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-clone.c.html#S_VolClone">Server implementation of VolClone()</a></pre>
      END_HTML
    */
    rc = VolClone(rpcid, ovolid, (RPC2_String)newvolname, (RPC2_Unsigned *)&newvolid);
    if (rc != RPC2_SUCCESS) {
	printf("VolClone failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolClone: New Volume id = %x\n", newvolid);
    printf("VolClone: New Volume name is %s\n", newvolname);
    exit(0);
}

static void printSFTPstats()
{
    printf("Sent: Total %d, Starts %d, Datas %d, Retries %d\n", sftp_Sent.Total,
	   sftp_Sent.Starts, sftp_Sent.Datas, sftp_Sent.DataRetries);
    printf("Sent: Acks %d, Naks %d, Busies %d, Bytes %d, Timeouts \n",
	   sftp_Sent.Acks, sftp_Sent.Naks, sftp_Sent.Busies, sftp_Sent.Bytes);

    printf("Recvd: Total %d, Starts %d, Datas %d, Retries %d\n", sftp_Recvd.Total,
	   sftp_Recvd.Starts, sftp_Recvd.Datas, sftp_Recvd.DataRetries);
    printf("Recvd: Acks %d, Naks %d, Busies %d, Bytes %d, Timeouts \n",
	   sftp_Recvd.Acks, sftp_Recvd.Naks, sftp_Recvd.Busies, sftp_Recvd.Bytes);
}

/*
  BEGIN_HTML
  <a name="dump"><strong>Client end of the <tt>dump</tt> request</strong></a> 
  END_HTML
*/
static void dump()
{
    long rc = 0;
    RPC2_Unsigned Incremental = 0;
    int err = 0;
    
    while ((these_args > 2) && *this_argp[2] == '-') {
	if (strcmp(this_argp[2], "-i") == 0) {
	    Incremental = 1;
	}
	else {
	    err = 1;
	}
	these_args--;
	this_argp++;
    }
    if (err || these_args != 4) {
	printf("Usage: volutil dump [-i] <volume-id> <file name>\n");
	exit(-1);
    }
    long volid;
    if (sscanf(this_argp[2], "%X", &volid) != 1){
	printf("Dump: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }

    /* Create lwp thread DumpLwp(argv[3]) */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    CODA_ASSERT(strlen(this_argp[3]) < sizeof(rock->dumpfile));
    strcpy(rock->dumpfile, this_argp[3]);
    rock->volid = volid;
    rock->numbytes = rock->fd = 0;
    
    PROCESS dumpPid;
    LWP_CreateProcess((PFIC)VolDumpLWP, 5 * 1024, LWP_NORMAL_PRIORITY,
		      (char *)rock, "VolDumpLWP", &dumpPid);
    
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-dump.c.html#S_VolNewDump">Server implementation of VolNewDump()</a></pre>
      END_HTML
      */
    rc = VolNewDump(rpcid, volid, &Incremental);
    if (rc != RPC2_SUCCESS) {
	printf("VolDump failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    printf("VolDump completed; %s output is in file %s\n",
	   Incremental?"Incremental":"", this_argp[3]);
    exit(0);
}


static void VolDumpLWP(struct rockInfo *rock)
{
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *myrequest;
    RPC2_Handle	mycid;
    register long rc;
    
    RPC2_SubsysIdent subsysid;
    FILE *tokfile;

    /* Hide the dumpfile name under a rock for later retrieval. */
    CODA_ASSERT(LWP_NewRock(ROCKTAG, (char *)rock) == LWP_SUCCESS);
    
    /* get encryption key for authentication */
    tokfile = fopen(TKFile, "r");
    fscanf(tokfile, "%s", vkey);
    fclose(tokfile);

    subsysid.Tag = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);
    
    myfilter.FromWhom = ONESUBSYS;
    myfilter.OldOrNew = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = VOLDUMP_SUBSYSTEMID;

    while (1) {
	rc=RPC2_GetRequest(&myfilter, &mycid, &myrequest, NULL, NULL, NULL, NULL);
	if (rc == RPC2_SUCCESS) {
	    rc = volDump_ExecuteRequest(mycid, myrequest, NULL);
	    if (rc) {
		printf("VolDumpLWP: request %d failed with %s\n",
			myrequest->Header.Opcode, RPC2_ErrorMsg((int)rc));
	    }
	}
	else printf("VolDumpLWP: Get_Request failed with %s\n",RPC2_ErrorMsg((int)rc));
    }

}

long WriteDump(RPC2_Handle rpcid, unsigned long offset, unsigned long *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    int status = 0;
    long rc = 0;
    struct rockInfo *rock;
    SE_Descriptor sed;
    
    CODA_ASSERT(LWP_GetRock(ROCKTAG, (char **)&rock) == LWP_SUCCESS);

    if (volid != rock->volid) {
	printf("Got a WriteDump for %x, I'm dumping %x!\n", volid, rock->volid);
	exit(-1);
    }

    if (rock->numbytes != offset) {
	printf("Offest %d != rock->numbytes %d\n", offset, rock->numbytes);
    }
    
    /* fetch the file with volume data */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sed.Value.SmartFTPD.ByteQuota = -1;
    sed.Value.SmartFTPD.SeekOffset = offset;
    sed.Value.SmartFTPD.hashmark = 0;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;
    CODA_ASSERT(strlen(rock->dumpfile) <
	   sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, rock->dumpfile);

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT){
	printf("WriteDump: Error %s in InitSideEffect\n", RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) 
	       <= RPC2_ELIMIT) {
	printf("WriteDump: Error %s in CheckSideEffect\n", RPC2_ErrorMsg((int)rc));
    }

    if (sed.Value.SmartFTPD.BytesTransferred != *nbytes) {
	printf("Transmitted bytes %d != requested bytes %d!\n",
	    *nbytes, sed.Value.SmartFTPD.BytesTransferred);
	*nbytes = sed.Value.SmartFTPD.BytesTransferred;
    }
    printf("Transmitted %d bytes.\n", sed.Value.SmartFTPD.BytesTransferred);
    rock->numbytes += sed.Value.SmartFTPD.BytesTransferred;
    return rc;
}


/*
  BEGIN_HTML
  <a name="restore"><strong>Client end of the <tt>restore</tt> request</strong></a> 
  END_HTML
*/
static void restorefromback()
{
    char volname[70];
    long rc = 0;
    long volid = 0;
    if (these_args < 4){
	printf("Usage: volutil restore <file name> <partition-name> [<volname> [<volid>]]\n");
	exit(-1);
    }
    if (these_args < 5) 
	bzero((void *)volname, 70);
    else
	strcpy(volname, this_argp[4]);

    if ((these_args < 6) || (sscanf(this_argp[5], "%X", &volid) == 0))
	volid = 0;

    /* Create lwp thread DumpLwp */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    CODA_ASSERT(strlen(this_argp[2]) < sizeof(rock->dumpfile));
    strcpy(rock->dumpfile, this_argp[2]);
    rock->volid = volid;
    rock->numbytes = 0;

    rock->fd = open(this_argp[2], O_RDONLY, 0);
    if (rock->fd < 0) {
	perror("RestoreFile");
	exit(-1);
    }
    
    PROCESS restorePid;
    LWP_CreateProcess((PFIC)VolDumpLWP, 5 * 1024, LWP_NORMAL_PRIORITY,
		      (char *)rock, "VolDumpLWP", &restorePid);
    if (rc != LWP_SUCCESS) {
	printf("VolDump can't create child %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-restore.c.html#S_VolRestore">Server implementation of VolRestore()</a></pre>
      END_HTML
      */
    
    rc = VolRestore(rpcid, (RPC2_String)this_argp[3], (RPC2_String)volname, (RPC2_Unsigned *)&volid);
    if (rc != RPC2_SUCCESS){
	printf("VolRestore failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    printf("VolRestore successful, created %#8x\n", volid);
    exit(0);
}

long ReadDump(RPC2_Handle rpcid, RPC2_Unsigned offset, RPC2_Integer *nbytes, VolumeId volid, SE_Descriptor *BD)
{
    int status = 0;
    long rc = 0;
    struct rockInfo *rock;
    SE_Descriptor sed;
    char *buf;
    
    CODA_ASSERT(LWP_GetRock(ROCKTAG, (char **)&rock) == LWP_SUCCESS);

    if (volid == 0) { /* User didn't assign one, use volId fileserver gives us. */
	rock->volid = volid;
    }
    
    if (volid != rock->volid) {
	printf("Got a ReadDump for %x, I'm reading %x!\n", volid, rock->volid);
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
    bzero((void *)&sed, sizeof(sed));
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
	printf("ReadDump: Error %s in InitSideEffect\n", RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) 
	       <= RPC2_ELIMIT) {
	printf("ReadDump: Error %s in CheckSideEffect\n", RPC2_ErrorMsg((int)rc));
    }

    printf("Transmitted %d bytes.\n", sed.Value.SmartFTPD.BytesTransferred);
    rock->numbytes += sed.Value.SmartFTPD.BytesTransferred;
    free(buf);
    return rc;
}

/*
  BEGIN_HTML
  <a name="dumpmem"><strong>Client end of the <tt>dumpmem</tt> request</strong></a> 
  END_HTML
*/
static void dumpmem() 
{
    if (these_args != 5) {
	printf("Usage: volutil dumpmem <file-name> <address> <size>\n");
	exit(-1);
    }
    int address, size;
    char *fname = this_argp[2];
    sscanf(this_argp[3], "%x", &address);
    sscanf(this_argp[4], "%d", &size);
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-dumpmem.c.html#S_VolDumpMem">Server implementation of VolDumpMem()</a></pre>
      END_HTML
    */
    rc = VolDumpMem(rpcid, (RPC2_String)fname, address, size);
    if (rc != RPC2_SUCCESS){
	printf("VolDumpMem failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Memory Dumped in file %s on server\n", fname);
    exit(0);
}


/*
  BEGIN_HTML
  <a name="rvmsize"><strong>Client end of the <tt>rvmsize</tt> request</strong></a> 
  END_HTML
*/
static void rvmsize()
{
    long volid;
    RVMSize_data data;
    
    if (these_args != 3) {
	printf("Usage: volutil rvmsize <volid>\n");
	exit(-1);
    }
    if (sscanf(this_argp[2], "%X", &volid) != 1){
	printf("RVMSize: Bad Volumeid %s\n", this_argp[2]);
	exit(-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-rvmsize.c.html#S_VolRVMSize">Server implementation of VolRVMSize()</a></pre>
      END_HTML
    */
    rc = VolRVMSize(rpcid, volid, &data);
    if (rc != RPC2_SUCCESS){
	printf("VolRVMSize failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %x used a total of %d bytes.\n", volid, data.VolumeSize);
    printf("\t%d small vnodes used %d bytes.\n", data.nSmallVnodes, data.SmallVnodeSize);
    printf("\t%d large vnodes used %d bytes.\n", data.nLargeVnodes, data.LargeVnodeSize);
    printf("\t and %d bytes of DirPages.\n", data.DirPagesSize);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="checkrec"><strong>Client end of the <tt>checkrec</tt> request<br>
  NOTE: THIS CODE IS COMMENTED OUT! </strong></a>
  END_HTML
*/
#ifdef notdef
static void checkrec() {
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-chkrec.c.html#S_VolChkRec">Server implementation of VolChkRec()</a></pre>
      END_HTML
    */
    rc = VolChkRec(rpcid, 0);
    if (rc != RPC2_SUCCESS) {
	printf("Check Recoverable Storage failed\n");
	exit(-1);
    }
    printf("Check Recoverable Storage found no corruption\n");
    exit(0);
}
static void dumprecstore()
{
    if (these_args > 4 || these_args < 3) {
	printf("Usage: volutil dumprecstore <file-name> <volumeid>\n");
	exit(-1);
    }
    char *fname = this_argp[2];
    int volid = 0;
    if (these_args == 4)
	sscanf(this_argp[3], "%x", &volid);
    rc = VolDumpRecStore(rpcid, volid, fname);
    if (rc != RPC2_SUCCESS){
	printf("VolDumpRecStore failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Recoverable storage Dumped in file %s on server\n", fname);
    exit(0);
}
#endif notdef
/*
  BEGIN_HTML
  <a name="backup"><strong>Client end of the <tt>backup</tt> request</strong></a> 
  END_HTML
*/
static void backup() {
    long Vid, backupVid;

    if (these_args != 3) {
	printf("Usage: volutil backup volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%X", &Vid) != 1){
	printf("VolMakeBackups: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-backup.c.html#S_VolMakeBackups">Server implementation of VolMakeBackups()</a></pre>
      END_HTML
    */
    rc = VolMakeBackups(rpcid, Vid, (VolumeId *)&backupVid);
    if (rc != RPC2_SUCCESS) {
	printf("VolMakeBackups failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Backup (id = %x) of Volume %x created \n", backupVid, Vid);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="create_rep"><strong>Client end of the <tt>create_rep</tt> request</strong></a> 
  END_HTML
*/
static void create_rep() {
    char *partition, *volumeName;
    long volumeid = 0;
    long groupid;

    if (these_args != 5 && these_args != 6) {
	printf("Usage:  volutil create_rep partition-path volumeName grpid [rw-volid]\n");
	exit(-1);
    }
    partition = this_argp[2];
    volumeName = this_argp[3];

#if 0 /* needs to be checked on the server */
    if (strncmp(partition, VICE_PARTITION_PREFIX, VICE_PREFIX_SIZE) != 0) {
	printf("Invalid partition specified, %s\n", (int)partition);
	exit(1);
    }
#endif
    if (sscanf(this_argp[4], "%X", &groupid) != 1){
	printf("CreateRep: Bad Group Id %s\n", this_argp[4]);
	exit(-1);
    }
    if (these_args == 6) {
	if (sscanf(this_argp[5], "%X", &volumeid) != 1){
	    printf("CreateRep: Bad Volume Id %s\n", this_argp[5]);
	    exit(-1);
	}
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-create.c.html#S_VolCreate">Server implementation of VolCreate()</a></pre>
      END_HTML
    */
    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName,
		   (RPC2_Unsigned *)&volumeid, 1, groupid);
    if (rc != RPC2_SUCCESS) {
	printf("VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %x (%s) created \n", volumeid, volumeName);
    exit(0);
}


/*
  BEGIN_HTML
  <a name="makevldb"><strong>Client end of the <tt>makevldb</tt> request</strong></a> 
  END_HTML
*/
static void makevldb() {
    char *infile;
    if (these_args != 3) {
	printf("Usage: volutil makevldb VolumeListFile\n");
	exit (-1);
    }
    infile = this_argp[2];
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-makevldb.c.html#S_VolMakeVLDB">Server implementation of VolMakeVLDB()</a></pre>
      END_HTML
    */
    rc = VolMakeVLDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
	printf("VolMakeVLDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VLDB completed.\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="makevrdb"><strong>Client end of the <tt>makevrdb</tt> request</strong></a> 
  END_HTML
*/
static void makevrdb() {
    char *infile;
    if (these_args != 3) {
	printf("Usage: volutil makevrdb VRListFile\n");
	exit (-1);
    }
    infile = this_argp[2];
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-makevrdb.c.html#S_VolMakeVRDB">Server implementation of VolMakeVRDB()</a></pre>
      END_HTML
    */
    rc = VolMakeVRDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
	printf("VolMakeVRDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VRDB completed.\n");
    exit(0);
}
/*
  BEGIN_HTML
  <a name="info"><strong>Client end of the <tt>info</tt> request</strong></a> 
  END_HTML
*/
static void info() {
    long DumpAll = 0;
    SE_Descriptor sed;
    int err = 0;

    if (these_args < 4) {
	printf("Usage: volutil info [-all] volumeName/volumeNumber outputfile\n");
	exit(-1);
    }
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
    if (err || these_args != 4) {
	printf("Usage: volutil info [-all] volumeName/volumeNumber outputfile\n");
	exit(-1);
    }

    sprintf(outfile, "%s", this_argp[3]);	// get output file name 

    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-info.c.html#S_VolInfo">Server implementation of VolInfo()</a></pre>
      END_HTML
    */
    rc = VolInfo(rpcid, (RPC2_String)this_argp[2], DumpAll, &sed);
    if (rc != RPC2_SUCCESS) {
	printf("VolInfo failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolInfo completed; results in %s\n", outfile);
    exit(0);
}
/*
  BEGIN_HTML
  <a name="showvnode"><strong>Client end of the <tt>showvnode</tt> request</strong></a>
  END_HTML
*/
static void showvnode() {
    SE_Descriptor sed;

    bit32 volumeNumber, vnodeNumber;
    bit32 unique;
    if (these_args != 6) {
	printf("Usage: volutil showvnode volumeNumber vnodeNumber Unique outputfile\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%X", &volumeNumber) != 1){
	printf("showvnode: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    if (sscanf(this_argp[3], "%X", &vnodeNumber) != 1){
	printf("showvnode: Bogus vnode number %s\n", this_argp[3]);
	exit(-1);
    }
    if (sscanf(this_argp[4], "%X", &unique) != 1) {
	printf("showvnode: Bogus Uniquifier %s\n", this_argp[4]);
	exit(-1);
    }
    sprintf(outfile, "%s", this_argp[5]);	// get output file name 
    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-showvnode.c.html#S_VolShowVnode">Server implementation of VolShowVnode()</a></pre>
      END_HTML
    */
    rc = VolShowVnode(rpcid, volumeNumber, vnodeNumber, unique, &sed);
    if (rc != RPC2_SUCCESS) {
	printf("VolShowVnode failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolShowVnode completed; output is in file %s\n", outfile);
    exit(0);
}
/*
  BEGIN_HTML
  <a name="setvv"><strong>Client end of the <tt>setvv</tt> request</strong></a> 
  END_HTML
*/
static void setvv() 
{
    bit32   volumeNumber, vnodeNumber;
    bit32   unique;
    ViceVersionVector	vv;

    if (these_args != 16){
	printf("Usage: volutil setvv volumeNumber vnodeNumber unique <version nubmers(8)> <StoreId (host) (Uniquifier)> <flags>\n");
	exit(-1);
    }
    
    if (sscanf(this_argp[2], "%X", &volumeNumber) != 1){
	printf("setvv: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    if (sscanf(this_argp[3], "%X", &vnodeNumber) != 1){
	printf("setvv: Bogus vnode number %s\n", this_argp[3]);
	exit(-1);
    }
    if (sscanf(this_argp[4], "%X", &unique) != 1) {
	printf("setvv: Bogus vnode uniquifier %s\n", this_argp[4]);
	exit(-1);
    }
    bzero((void *)&vv, sizeof(vv));
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
    
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-setvv.c.html#S_VolSetVV">Server implementation of VolSetVV()</a></pre>
      END_HTML
    */
    rc = VolSetVV(rpcid, volumeNumber, vnodeNumber, unique, &vv);
    if (rc != RPC2_SUCCESS){
	printf("VolSetVV failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolSetVV completed\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="purge"><strong>Client end of the <tt>purge</tt> request</strong></a> 
  END_HTML
*/
static void purge() {
    long rc;
    VolumeId volid = 0;

    if (these_args != 4) {
	printf("Usage: volutil purge VolumeId VolumeName\n");
	exit(-1);
    }
/*
    volid = (bit32) atol(this_argp[2]);
*/
    if (sscanf(this_argp[2], "%X", &volid) != 1){
	printf("Purge: Bad Volume Id %s\n", this_argp[2]);
	exit(-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-purge.c.html#S_VolPurge">Server implementation of VolPurge()</a></pre>
      END_HTML
    */
    rc = VolPurge(rpcid, volid, (RPC2_String)this_argp[3]);
    if (rc != RPC2_SUCCESS) {
	printf("VolPurge failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %x (%s) successfully purged\n", volid, this_argp[3]);
    exit(0);
}
/*
  BEGIN_HTML
  <a name="lock"><strong>Client end of request to <tt>lock</tt> a volume</strong></a>
  END_HTML
*/
static void lock() {
    long Vid;
    ViceVersionVector	vvv;
    long rc;
    
    if (these_args != 3) {
	printf("Usage: volutil lock volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%X", &Vid) != 1){
	printf("VolLock: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-lock.c.html#S_VolLock">Server implementation of VolLock()</a></pre>
      END_HTML
    */
    rc = VolLock(rpcid, Vid, &vvv);
    if (rc != RPC2_SUCCESS) {
	printf("VolLock failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Locked volume %x had a VVV of (%d,%d,%d,%d,%d,%d,%d,%d)\n",
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
static void unlock() {
    long Vid;
    
    if (these_args != 3) {
	printf("Usage: volutil unlock volumeId\n");
	exit(-1);
    }

    if (sscanf(this_argp[2], "%X", &Vid) != 1){
	printf("VolUnlock: Bogus volume number %s\n", this_argp[2]);
	exit(-1);
    }

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-lock.c.html#S_VolUnlock">Server implementation of VolUnlock()</a></pre>
      END_HTML
    */
    rc = VolUnlock(rpcid, Vid);
    if (rc != RPC2_SUCCESS) {
	printf("VolUnlock failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Volume %x is unlocked.\n", Vid);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="lookup"><strong>Client end of request to <tt>lookup</tt> a volume</strong></a>
  END_HTML
*/
static void lookup() {
    SE_Descriptor sed;

    if (these_args != 4) {
	printf("Usage: volutil lookup <volume name or id> outputfile\n");
	exit(-1);
    }

    sprintf(outfile, "%s", this_argp[3]);   // get output file name 

    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-lookup.c.html#S_VolLookup">Server implementation of VolLookup()</a></pre>
      END_HTML
    */
    rc = VolLookup(rpcid, (RPC2_String)this_argp[2], &sed);
    if (rc != RPC2_SUCCESS) {
	printf("VolLookup failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolLookup completed; output is in file %s\n", outfile);
    exit(0);
    
}
/*
  BEGIN_HTML
  <a name="updatedb"><strong>Client end of the <tt>updatedb()</tt> request</strong> </a>
  END_HTML
 */
static void updatedb() {
    if (these_args != 2) {
	printf("Usage: volutil updatedb\n");
	exit (-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="volutil.c.html#S_VolUpdateDB">Server implementation of VolUpdateDB()</a></pre>
      END_HTML
    */
    
    rc = VolUpdateDB(rpcid);
    if (rc != RPC2_SUCCESS) {
	printf("VolUpdateDB failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Databases updated on host %s.\n", s_hostname);
    exit(0);
}

/*
  BEGIN_HTML
  <a name="shutdown"><strong>Client end of the <tt>shutdown</tt> request</strong></a> 
  END_HTML
*/
static void shutdown() {
    if (these_args != 2) {
	printf("Usage: volutil shutdown\n");
	exit (-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="volutil.c.html#S_VolShutdown">Server implementation of VolShutdown()</a></pre>
      END_HTML
      */
    rc = VolShutdown(rpcid);
    if (rc != RPC2_SUCCESS) {
	printf("VolShutdown failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Fileserver shutdown.\n");
    exit(0);
}
/*
  BEGIN_HTML
  <a name="swaplog"><strong>Client end of the <tt>swaplog</tt> request</strong></a> 
  END_HTML
*/
static void swaplog() {
    if (these_args != 2) {
	printf("Usage: volutil swaplog\n");
	exit (-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="volutil.c.html#S_VolSwaplog">Server implementation of VolSwaplog()</a></pre>
      END_HTML
    */
    
    rc = VolSwaplog(rpcid);
    if (rc != RPC2_SUCCESS) {
	printf("VolSwaplog failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Fileserver log successfully swapped.\n");
    exit(0);
}


/*
  BEGIN_HTML
  <a name="swapmalloc"><strong>Client end of the <tt>togglemalloc</tt> request</strong></a> 
  END_HTML
*/
static void swapmalloc() {
    if (these_args != 2) {
	printf("Usage: volutil togglemalloc\n");
	exit (-1);
    }
    /*
      BEGIN_HTML
      <pre>
      <a href="volutil.c.html#S_VolSwapmalloc">Server implementation of VolSwapmalloc()</a></pre>
      END_HTML
    */
    
    rc = VolSwapmalloc(rpcid);
    if (rc != RPC2_SUCCESS) {
	printf("VolSwapmalloc failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Malloc tracing successfuly toggled.\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="setdebug"><strong>Client end of the <tt>setdebug()</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void setdebug() {
    int debuglevel = 0;

    if (these_args != 3) {
	printf("Usage: volutil setdebug debuglevel\n");
	exit (-1);
    }
    debuglevel = atoi(this_argp[2]);
    /*
      BEGIN_HTML
      <pre>
      <a href="volutil.c.html#S_VolSetDebug">Server implementation of VolSetDebug()</a></pre>
      END_HTML
    */
    rc = VolSetDebug(rpcid, debuglevel);
    if (rc != RPC2_SUCCESS) {
	printf("VolSetDebug failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("VolumeDebugLevel set to %d.\n", debuglevel);
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="timing"><strong>Client end of the <tt>timing</tt> request</strong></a>
  </pre>
  END_HTML
*/
void timing() {
    int on = -1;
    char filename[256];
    SE_Descriptor sed;

    if (these_args != 3 && these_args != 4) {
	printf("Usage: volutil timing <on | off filename>\n");
	exit(-1);
    }
    
    if (strcmp(this_argp[2], "on") == 0)  {
	on = 1;
	strcpy(filename, "/tmp/XXXXXX");
	mktemp(filename);
    }
    else if (strcmp(this_argp[2], "off") == 0) {
	on = 0;
	CODA_ASSERT(these_args == 4);
	strcpy(filename, this_argp[3]);
    }

    if (on == -1) { 
	printf("Usage: volutil timing <on | off filename>\n");
	exit(-1);
    }
    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(filename) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, filename, strlen(filename));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-timing.c.html#S_VolTiming">Server implementation of VolTiming()</a></pre>
      END_HTML
    */
    rc = VolTiming(rpcid, on, &sed);
    
    if (rc != RPC2_SUCCESS) {
	printf("VolTiming failed with return code %d\n", rc);
	exit(-1);
    }
    printf("Timing finished successfully\n");
    if (!on) 
	printf("Output is in %s\n", filename);
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="elapse"><strong>Client end of the <tt>elapse</tt> request</strong></a>
  </pre>
  END_HTML
*/
void elapse() {
    int on = -1;
    int subid = -1;
    int multi = -1;

    if (these_args != 4 && these_args != 5) {
	printf("Usage: volutil elapse <on | off> subsystem-name [MultiRPC]\n");
	exit(-1);
    }
    
    if (strcmp(this_argp[2], "on") == 0) on = 1;
    else if (strcmp(this_argp[2], "off") == 0) on = 0;
    else {
	printf("Usage: volutil elapse <on | off> subsystem-name [MultiRPC]\n");
	exit(-1);
    }

    if (strcmp(this_argp[3], "resolution") == 0)  {
	subid = VELAPSE_RESOLUTION;
    } else if (strcmp(this_argp[3], "cb") == 0) {
	subid = VELAPSE_CB;
    } else if (strcmp(this_argp[3], "mond") == 0) {
	subid = VELAPSE_MOND;
    } else if (strcmp(this_argp[3], "volDump") == 0) {
	subid = VELAPSE_VOLDUMP;
    } else {
	printf("Switch name must be chosen among resolution, cb, mond, and volDump\n");
	exit(-1);
    }

    if (these_args == 5) {
        if (strcmp(this_argp[4], "MultiRPC") == 0 || strcmp(this_argp[4], "M") == 0)  {
	    multi = 1;
        } else {
	    printf("To collect MultiRPC elapse time, specify \"MultiRPC\" or \"M\"\n");
	    exit(-1);
        }
    } else multi = 0;


    /*
      BEGIN_HTML
      <pre>
      <a href="vol-elapse.c.html#S_VolElapse">Server implementation of VolElapse()</a></pre>
      END_HTML
    */
    rc = VolElapse(rpcid, on, subid, multi);
    
    if (rc != RPC2_SUCCESS) {
	printf("VolElapse failed with return code %d\n", rc);
	exit(-1);
    }
    printf("VolElapse finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="tracerpc"><strong>Client end of the <tt>tracerpc</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void tracerpc() {
    SE_Descriptor sed;
    if (these_args != 3) {
	printf("Usage: volutil tracerpc outputfile\n");
	exit(-1);
    }

    sprintf(outfile, "%s", this_argp[2]);	// get output file name 

    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-tracerpc.c.html#S_TraceRpc">Server implementation of TraceRpc()</a></pre>
      END_HTML
    */
    rc = TraceRpc(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
	printf("TraceRpc failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }

    printf("TraceRpc finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="printstats"><strong>Client end of the <tt>printstats</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void printstats() {
    SE_Descriptor sed;
    if (these_args != 3) {
	printf("Usage: volutil printstats outputfile\n");
	exit(-1);
    }

    sprintf(outfile, "%s", this_argp[2]);	// get output file name 

    /* set up side effect descriptor */
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-printstats.c.html#S_PrintStats">Server implementation of PrintStats()</a></pre>
      END_HTML
    */
    rc = PrintStats(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
	printf("PrintStats failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("PrintStats finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <pre>
  <a name="showcallbacks"><strong>Client end of the <tt>showcallbacks</tt> request</strong></a>
  </pre>
  END_HTML
*/
static void showcallbacks() {
    if (these_args != 6) {
	printf("Usage: volutil showcallbacks <volumeid> <vnode> <unique> <out-file>\n");
	exit(-1);
    }
    
    sprintf(outfile, "%s", this_argp[5]);
    ViceFid fid;
    if ((sscanf(this_argp[2], "%X", &fid.Volume) != 1) ||
	(sscanf(this_argp[3], "%X", &fid.Vnode) != 1) ||
	(sscanf(this_argp[4], "%X", &fid.Unique) != 1)) {
	printf("Usage: volutil showcallbacks <volumeid> <vnode> <unique> <out-file>\n");
	exit(-1);
    }
    /* set up side effect descriptor */
    SE_Descriptor sed;
    bzero((void *)&sed, sizeof(sed));
    sed.Tag = SMARTFTP;
    sed.Value.SmartFTPD.Tag = FILEBYNAME;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    CODA_ASSERT(strlen(outfile) < sizeof(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName));
    strncpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, outfile, strlen(outfile));

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-showcallbacks.c.html#S_ShowCallbacks">Server implementation of showcallbacks()</a></pre>
      END_HTML
    */    rc = ShowCallbacks(rpcid, &fid, &sed);
    if (rc != RPC2_SUCCESS) {
	printf("Showcallbacks failed with %s\n", RPC2_ErrorMsg((int)rc));
	exit(-1);
    }
    printf("Showcallbacks finished successfully\n");
    exit(0);
}

/*
  BEGIN_HTML
  <a name="truncatervmlog"><strong>Client end of the <tt>truncatervmlog</tt> request</strong></a> 
  END_HTML
*/
static void truncatervmlog() {
    /*
      BEGIN_HTML
      <pre>
      <a href="vol-rvmtrunc.c.html#S_TruncateRVMLog">Server implementation of TruncateRVMLog()</a></pre>
      END_HTML
    */
    rc = TruncateRVMLog(rpcid);
    if (rc != RPC2_SUCCESS) {
	printf("Couldn't truncate log\n");
	exit(-1);
    }
    printf("Truncate of RVM log started....Wait for a few minutes for it to complete\n");
    exit(0);
}


/*
  BEGIN_HTML
  <a name="getmaxvol"><strong>Client end of the getmaxvol request</strong></a> 
  END_HTML
*/
static void getmaxvol() {
    VolumeId maxid;

    if (these_args != 2) {
	printf("Usage: volutil getmaxvol\n");
	exit(-1);
    }

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-maxid.c.html#S_VolGetMaxVolId">Server implementation of VolGetMaxVolId()</a></pre>
      END_HTML
    */
    rc = VolGetMaxVolId(rpcid, (RPC2_Integer *)&maxid);
    if (rc != RPC2_SUCCESS) { 
        printf("Couldn't get maxvolid: %s\n", RPC2_ErrorMsg((int)rc));
	exit (-1);
    }
    printf("Maximum volume id is 0x%X\n", maxid);
    exit (0);
}


/*
  BEGIN_HTML
  <a name="setmaxvol"><strong>Client end of the setmaxvol request</strong></a> 
  END_HTML
*/
static void setmaxvol() {
    VolumeId volid;

    if ((these_args != 3) || (sscanf(this_argp[2], "%X", &volid) != 1)) {
        printf("Usage: volutil setmaxvol <volumeid>\n");
	exit(-1);
    }

    /*
      BEGIN_HTML
      <pre>
      <a href="vol-maxid.c.html#S_VolSetMaxVolId">Server implementation of VolSetMaxVolId()</a></pre>
      END_HTML
    */
    rc = VolSetMaxVolId(rpcid, volid);
    if (rc != RPC2_SUCCESS) {
        printf("Couldn't set new maximum volume id, check that you didn't try\n");
	printf("to change the server id or set a new maxid that is less than\n");
	printf("the current maximum id.\n");
	exit(-1);
    }

    printf("Maximum volume id set to 0x%x\n", volid);
    exit(0);
}

static void peekpokeerr()
{
	static	char	*msgs[]={
		/*0*/ "Path to server file not known",
		/*1*/ "Cannot read symbols from the server file",
		/*2*/ "Symbol not found in the server file",
		/*3*/ "Address not in server virtual space",
		/*4*/ "Memory protection error",
		/*5*/ "Unaligned integer/pointer reference"};
	printf("Couldn't %s at %s: %s\n", this_argp[1], this_argp[2],
	       rc >= -10040L && rc < sizeof(msgs)/sizeof(*msgs)-10040L ?
	       msgs[10040 + (int) rc] : RPC2_ErrorMsg((int) rc));
	exit (-1);
}

static void usageerr(char *args)
{
	printf("Usage: %s %s %s\n", this_argp[0], this_argp[1], args);
	exit(-1);
}

static int sscani(char *s, RPC2_Integer *px)
{
	long	k;
	int	q;
	while(isspace(*s)) s++;
	if (*s != '0')
		q = sscanf(s, "%ld", &k);
	else if (*++s != 'x')
		q = sscanf(s, "%lo", &k);
	else	q = sscanf(++s, "%lx", &k);
	*px = (RPC2_Integer) k;
	return(q);
}

/*
  BEGIN_HTML
  <a name="peekint"><strong>Client end of the peek request</strong></a> 
  END_HTML
*/
static void peekint() {
	RPC2_Integer value;

	if (these_args != 3) usageerr("<address>");
	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPeekInt">Server implementation of VolPeekInt()</a></pre>
	  END_HTML
	*/
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
static void pokeint() {
	RPC2_Integer value;

	if ((these_args != 4) || (sscani(this_argp[3], &value) != 1))
		usageerr("<address> <value>");

	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPokeInt">Server implementation of VolPokeInt()</a></pre>
	  END_HTML
	*/
	if ((rc = VolPokeInt(rpcid, (RPC2_String) this_argp[2], value)) != RPC2_SUCCESS)
		peekpokeerr();
	printf("0x%lx stored at %s\n", (long) value, this_argp[2]);
	exit(0);
}

/*
  BEGIN_HTML
  <a name="peekmem"><strong>Client end of the <tt>peeks()</tt> request</strong></a> 
  END_HTML
*/
static void peekmem() {
	RPC2_BoundedBS buf;

	if ((these_args != 4) || (sscani(this_argp[3], &buf.MaxSeqLen) != 1))
		usageerr("<address> <size>");

	if ((buf.SeqBody = (RPC2_String) malloc((int) buf.MaxSeqLen + 1)) == NULL) {
		printf("volutil: Out of memory\n");
		exit(-1);
	}
	buf.SeqLen = 0;
	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPeekMem">Server implementation of VolPeekMem()</a></pre>
	  END_HTML
	*/
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
static void pokemem() {
	RPC2_CountedBS buf;

	if ((these_args != 5) || (sscani(this_argp[3], &buf.SeqLen) != 1))
		usageerr("<address> <size> <value>");

	buf.SeqBody = (RPC2_String) this_argp[4];
	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPokeMem">Server implementation of VolPokeMem()</a></pre>
	  END_HTML
	*/
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
static void peekxmem() {
	RPC2_BoundedBS buf;

	if ((these_args != 4) || (sscani(this_argp[3], &buf.MaxSeqLen) != 1))
		usageerr("<address> <size>");

	if ((buf.SeqBody = (RPC2_String) malloc((int) buf.MaxSeqLen)) == NULL) {
		printf("volutil: Out of memory\n");
		exit(-1);
	}
	buf.SeqLen = 0;
	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPeekMem">Server implementation of VolPeekMem()</a></pre>
	  END_HTML
	*/
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
static void pokexmem() {
	RPC2_CountedBS buf;
	char	*t, *s;
	RPC2_Integer size;

	if ((these_args != 5) || (sscani(this_argp[3], &buf.SeqLen) != 1))
		usageerr("<address> <size> <hexvalue>");

	if ((buf.SeqBody = (RPC2_String) malloc((int) buf.SeqLen)) == NULL) {
		printf("volutil: Out of memory\n");
		exit(-1);
	}
	if ((s = this_argp[4])[0] == '0' && s[1] == 'x') s += 2;
	t = (char *) buf.SeqBody;
	size = buf.SeqLen;
	while(size--) {
		int	vh, vl;
		if (!isxdigit(s[0]) || !isxdigit(s[1])) {
			printf("%s is not a %s-byte hex string\n",
			       this_argp[4], this_argp[3]);
			usageerr("<address> <size> <hexvalue>");
		}
		vh = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
		s++ ;
		vl = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
		s++ ;
		*t++ = (vh << 4) + vl;
	}

	/*
	  BEGIN_HTML
	  <pre>
	  <a href="vol-peekpoke.c.html#S_VolPokeMem">Server implementation of VolPokeMem</a></pre>
	  END_HTML
	*/
	if ((rc = VolPokeMem(rpcid, (RPC2_String) this_argp[2], &buf)) != RPC2_SUCCESS)
		peekpokeerr();
	printf("0x%s stored at %s\n",
	       (this_argp[4][0] == '0' && this_argp[4][1] == 'x') ?
	       this_argp[4] + 2 : this_argp[4],
	       this_argp[2]);
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
    tokfile = fopen(TKFile, "r");
    if (!tokfile) {
	char estring[80];
	sprintf(estring, "Tokenfile %s", TKFile);
	perror(estring);
	exit(-1);
    }
    fscanf(tokfile, "%s", vkey);
    fclose(tokfile);

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY-1, &mylpid) == LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi);
    tout.tv_sec = timeout;
    tout.tv_usec = 0;
    rcode = RPC2_Init(RPC2_VERSION, 0, NULL, 3, &tout);
    if (rcode != RPC2_SUCCESS) {
	printf("RPC2_Init failed with %s\n", RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}


static int V_BindToServer(char *fileserver, RPC2_Handle *RPCid)
{
 /* Binds to File Server on volume utility port on behalf of uName.
    Sets RPCid to the value of the connection id.    */

    RPC2_HostIdent hident;
    RPC2_PortalIdent pident;
    RPC2_SubsysIdent sident;
    long     rcode;

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, fileserver);
#ifdef __CYGWIN32__
	/* XXX -JJK */
	pident.Tag = RPC2_PORTALBYINETNUMBER;
	pident.Value.InetPortNumber = htons(PORT_codasrv);
#else
    pident.Tag = RPC2_PORTALBYNAME;
    strcpy(pident.Value.Name, "codasrv");
#endif
    sident.Tag = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = UTIL_SUBSYSID;

    printf("V_BindToServer: binding to host %s\n", fileserver);
    RPC2_BindParms bparms;
    bzero((void *)&bparms, sizeof(bparms));
    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SideEffectType = SMARTFTP;

    rcode = RPC2_NewBinding(&hident, &pident, &sident, &bparms, RPCid);
    if (rcode < 0 && rcode > RPC2_ELIMIT)
	rcode = 0;
    if (rcode == 0 || rcode == RPC2_NOTAUTHENTICATED)
	return(rcode);
    else {
	printf("RPC2_NewBinding to server %s failed with %s\n",
				fileserver, RPC2_ErrorMsg((int)rcode));
	exit(-1);
    }
}

static void Die (char *msg)
{
    printf("%s\n", msg);
    CODA_ASSERT(0);
}

