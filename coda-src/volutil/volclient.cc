/* BLURB gpl

                           Coda File System
                              Release 7

          Copyright (c) 1987-2019 Carnegie Mellon University
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
#include <vice.h>
#include <callback.h>
#include <volutil.h>
#include <voldump.h>
#include <auth2.h>
#include <avenus.h>

#ifdef __cplusplus
}
#endif

#include <vice_file.h>
#include <cvnode.h>
#include <volume.h>

#include <codaconf.h>
#include <vice_file.h>
#include <getsecret.h>
#include <coda_getservbyname.h>

static const char *vicedir = NULL;

/* hack to make argc and argv visible to subroutines */
static char **this_argp;
static int these_args;

static char s_hostname[100];
static RPC2_Handle rpcid;
static long rc;

static void backup(void);
static void salvage(void);
static void create(void);
static void create_rep(void);
static void clone(void);
static void makevldb(void);
static void makevrdb(void);
static void dumpvrdb(void);
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

#define ROCKTAG 12345
struct rockInfo {
    int fd; /* Open filedescriptor for ReadDump/WriteDump. */
    VolumeId volid; /* Volume being dumped. */
    unsigned int numbytes; /* Number of bytes already written to the file.
			       (has to wrap around the same way as 'offset') */
};

static void V_InitRPC(int timeout);
static int V_BindToServer(char *fileserver, char *realm, RPC2_Handle *RPCid);
static void VolDumpLWP(void *arg);
extern long volDump_ExecuteRequest(RPC2_Handle, RPC2_PacketBuffer *,
                                   SE_Descriptor *);

void ReadConfigFile(void)
{
    /* Load configuration file to get vice dir. */
    codaconf_init("server.conf");

    CODACONF_STR(vicedir, "vicedir", "/vice");

    vice_dir_init(vicedir);
}

int main(int argc, char **argv)
{
    char *realm = NULL;
    int timeout = 30; /* Default rpc2 timeout is 30 seconds. */

    /* Set the default server host */
    gethostname(s_hostname, sizeof(s_hostname) - 1);

    ReadConfigFile();

    while (argc > 2 && *argv[1] == '-') { /* All options require an argument. */
        if (strcmp(argv[1], "-h") == 0) { /* User specified other host. */
            struct hostent *hp;
            hp = gethostbyname(argv[2]);
            if (!hp) {
                fprintf(stderr, "%s is not a valid host name.\n", argv[2]);
                exit(EXIT_FAILURE);
            }
            strcpy(s_hostname, hp->h_name);
        } else if (strcmp(argv[1], "-r") == 0) { /* User specified realm. */
            realm = argv[2];
        } else if (strcmp(argv[1], "-t") == 0) { /* timeout */
            timeout = atoi(argv[2]);
        } else if (strcmp(argv[1], "-d") == 0) { /* debuglevel */
            RPC2_DebugLevel = atoi(argv[2]);
            VolDebugLevel   = atoi(argv[2]);
        }
        argv++;
        argc--;
        argv++;
        argc--;
    }

    if (argc < 2)
        goto bad_options;

    CODA_ASSERT(*s_hostname != '\0');
    V_InitRPC(timeout);
    V_BindToServer(s_hostname, realm, &rpcid);

    this_argp  = argv;
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
    else if (strcmp(argv[1], "dumpvrdb") == 0)
        dumpvrdb();
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
    else
        goto bad_options;

    return 0;

bad_options:
    fprintf(
        stderr,
        "Usage: volutil [-h host] [-r realm] [-t timeout] [-d debuglevel] <option>,\n"
        "    where <option> is one of the following:\n"
        "\tancient, backup, create, create_rep, clone, dump, dumpestimate,\n"
        "\trestore, info, lock, lookup, makevldb, makevrdb, purge, salvage,\n"
        "\tsetvv, showvnode, shutdown, swaplog, setdebug, updatedb, unlock,\n"
        "\tdumpmem, rvmsize, timing, printstats,\n"
        "\tshowcallbacks, truncatervmlog,togglemalloc, getmaxvol, setmaxvol,\n"
        "\tpeek, poke, peeks, pokes, peekx, pokex, setlogparms, tracerpc\n"
        "\tgetvolumelist, dumpvrdb\n");
    exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[2], "%lX", &backupid) != 1) {
        fprintf(stderr, "MarkAsAncient: Bad backupId %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = NewVolMarkAsAncient(rpcid, backupid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolMarkAsAncient failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS); /* Funny, need to exit or the program never exits... */
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
    flag     = -1;

    if (these_args < 5) {
        fprintf(
            stderr,
            "Usage: volutil setlogparms <volid> reson <flag> logsize <nentries>\n");
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[2], "%lX", &volid) != 1) {
        fprintf(stderr, "setlogparms: Bad VolumeId %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }
    for (i = 3; i < these_args; i++) {
        if (strcmp(this_argp[i], "reson") == 0) {
            i = i + 1;
            if (sscanf(this_argp[i], "%ld", &flag) != 1) {
                fprintf(stderr, "Bad flag value %s\n", this_argp[i]);
                exit(EXIT_FAILURE);
            }
        }
        if (strcmp(this_argp[i], "logsize") == 0) {
            i = i + 1;
            if (sscanf(this_argp[i], "%ld", &nentries) != 1) {
                fprintf(stderr, "Bad logsize value %s\n", this_argp[i]);
                exit(EXIT_FAILURE);
            }
        }
    }

    rc = VolSetLogParms(rpcid, volid, flag, nentries);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSetLogParms failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Set Log parameters\n");
    exit(EXIT_SUCCESS);
}

/**
 * salvage
 *
 * The salvage option to volutil doesn't work right. Please don't try it.
 */
static void salvage(void)
{
    int err             = 0;
    int debug           = 0; /* -d flag */
    int listinodeoption = 0;
    int forcesalvage    = 0;
    VolumeId vid        = 0;
    char *path          = NULL;

    if (these_args < 3) {
        fprintf(stderr,
                "Usage: volutil salvage [-d][-f][-i] partition "
                "[rw-vol number]\n"
                "The salvage option to volutil doesn't work right. "
                "Please don't try it.\n");
        exit(EXIT_FAILURE);
    }
    these_args--;
    this_argp++;
    these_args--;
    this_argp++;
    while (these_args && **this_argp == '-') {
        if (strcmp(*this_argp, "-d") == 0)
            debug = 1;
        else if (strcmp(*this_argp, "-t") == 0) {
            fprintf(stderr, "Testing option not implemented\n");
            exit(EXIT_FAILURE);
        } else if (strcmp(*this_argp, "-i") == 0)
            listinodeoption = 1;
        else if (strcmp(*this_argp, "-f") == 0)
            forcesalvage = 1;
        else
            err = 1;
        these_args--;
        this_argp++;
    }
    if (err || these_args > 2) {
        fprintf(
            stderr,
            "Usage: volutil salvage [-d] [-f] [-i] partition [read/write-volume-number]\n");
        exit(EXIT_FAILURE);
    }
    if (these_args > 0)
        path = this_argp[0];
    if (these_args == 2) {
        if (sscanf(this_argp[1], "%x", &vid) != 1) {
            fprintf(stderr,
                    "salvage: invalid volume id specified; salvage aborted\n");
            exit(EXIT_FAILURE);
        }
    }

    rc = VolSalvage(rpcid, (RPC2_String)path, vid, forcesalvage, debug,
                    listinodeoption);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSalvage failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Salvage complete.\n");
    exit(EXIT_SUCCESS);
}

static void stripslash(char *partition)
{
    char *end;
    if (strlen(partition) > 2) {
        end = partition + strlen(partition) - 1;
        if (*end == '/')
            *end = '\0';
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
    VolumeId volumeid = 0;

    if (these_args != 4) {
        fprintf(stderr, "Usage:  volutil create partition-path volumeName\n");
        exit(EXIT_FAILURE);
    }
    partition = this_argp[2];
    stripslash(partition);
    volumeName = this_argp[3];

    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName,
                   &volumeid, 0, 0);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Volume %08x (%s) created \n", volumeid, volumeName);
    exit(EXIT_SUCCESS);
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
        fprintf(stderr,
                "Usage: volutil clone <volume-id> [-n <new volume name>]\n");
        exit(EXIT_FAILURE);
    }
    VolumeId ovolid, newvolid;
    char buf[1];
    buf[0]           = '\0';
    char *newvolname = buf;
    long rc;

    if (sscanf(this_argp[2], "%x", &ovolid) != 1) {
        fprintf(stderr, "Clone: Bad Volumeid %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }
    if (these_args == 5) {
        if (!strcmp(this_argp[3], "-n"))
            newvolname = this_argp[4];
    }

    rc = VolClone(rpcid, ovolid, (RPC2_String)newvolname, &newvolid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolClone failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("VolClone: New Volume id = %08x\n", newvolid);
    printf("VolClone: New Volume name is %s\n", newvolname);
    exit(EXIT_SUCCESS);
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
    long rc                  = 0;
    RPC2_Integer Incremental = 0;
    int err                  = 0;
    FILE *outf;

    while ((these_args > 2) && *this_argp[2] == '-') {
        if (strcmp(this_argp[2], "-i") == 0) {
            if (these_args > 4) {
                Incremental = atoi(this_argp[3]);
                these_args--;
                this_argp++;
            } else
                Incremental = 1;
        } else
            err = 1;

        these_args--;
        this_argp++;
    }
    if (err || these_args < 3) {
        fprintf(stderr, "Usage: volutil dump [-i [lvl]] <volume-id> [file]\n");
        exit(EXIT_FAILURE);
    }

    long volid;
    if (sscanf(this_argp[2], "%lX", &volid) != 1) {
        fprintf(stderr, "Dump: Bad Volumeid %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    if (these_args < 4)
        outf = stdout;
    else
        outf = fopen(this_argp[3], "w");

    /* Create lwp thread DumpLwp(argv[3]) */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    rock->fd              = fileno(outf);
    rock->volid           = volid;
    rock->numbytes        = 0;

    PROCESS dumpPid;
    LWP_CreateProcess(VolDumpLWP, 16 * 1024, LWP_NORMAL_PRIORITY, (void *)rock,
                      "VolDumpLWP", &dumpPid);

    rc = VolNewDump(rpcid, volid, &Incremental);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "\nVolDump failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "\n%sVolDump completed, %u bytes dumped\n",
            Incremental ? "Incremental " : "", rock->numbytes);
    exit(EXIT_SUCCESS);
}

/**
 * dumpestimate - estimate the size of a volume dump
 * @volumeid:	volume replica id
 */
static void dumpestimate(void)
{
    int rc = 0;
    VolumeId volid;
    VolDumpEstimates sizes;

    if (these_args < 3) {
        fprintf(stderr, "Usage: volutil dumpestimate <volume-id>\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &volid) != 1) {
        fprintf(stderr, "Dump: Bad Volumeid %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolDumpEstimate(rpcid, volid, &sizes);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolDumpEstimate failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }

    printf("Level0> %u %u %u %u %u %u %u %u %u %u <Level9\n", sizes.Lvl0,
           sizes.Lvl1, sizes.Lvl2, sizes.Lvl3, sizes.Lvl4, sizes.Lvl5,
           sizes.Lvl6, sizes.Lvl7, sizes.Lvl8, sizes.Lvl9);
    fprintf(stderr, "VolDumpEstimate completed\n");

    exit(EXIT_SUCCESS);
}

static void VolDumpLWP(void *arg)
{
    struct rockInfo *rock = (struct rockInfo *)arg;
    RPC2_RequestFilter myfilter;
    RPC2_PacketBuffer *myrequest;
    RPC2_Handle mycid;
    long rc;

    RPC2_SubsysIdent subsysid;

    /* Hide the dumpfile name under a rock for later retrieval. */
    CODA_ASSERT(LWP_NewRock(ROCKTAG, (char *)rock) == LWP_SUCCESS);

    subsysid.Tag            = RPC2_SUBSYSBYID;
    subsysid.Value.SubsysId = VOLDUMP_SUBSYSTEMID;
    CODA_ASSERT(RPC2_Export(&subsysid) == RPC2_SUCCESS);

    myfilter.FromWhom              = ONESUBSYS;
    myfilter.OldOrNew              = OLDORNEW;
    myfilter.ConnOrSubsys.SubsysId = VOLDUMP_SUBSYSTEMID;

    while (1) {
        rc =
            RPC2_GetRequest(&myfilter, &mycid, &myrequest, NULL, NULL, 0, NULL);
        if (rc == RPC2_SUCCESS) {
            rc = volDump_ExecuteRequest(mycid, myrequest, NULL);
            if (rc) {
                fprintf(stderr, "VolDumpLWP: request %d failed with %s\n",
                        myrequest->Header.Opcode, RPC2_ErrorMsg((int)rc));
            }
        } else
            fprintf(stderr, "VolDumpLWP: Get_Request failed with %s\n",
                    RPC2_ErrorMsg((int)rc));
    }
}

long S_WriteDump(RPC2_Handle rpcid, RPC2_Unsigned offset, RPC2_Unsigned *nbytes,
                 VolumeId volid, SE_Descriptor *BD)
{
    long rc = 0;
    struct rockInfo *rockinfo;
    SE_Descriptor sed;
    char *rock;

    CODA_ASSERT(LWP_GetRock(ROCKTAG, &rock) == LWP_SUCCESS);
    rockinfo = (struct rockInfo *)rock;

    if (volid != rockinfo->volid) {
        fprintf(stderr, "Got a WriteDump for %08x, I'm dumping %08x!\n", volid,
                rockinfo->volid);
        exit(EXIT_FAILURE);
    }

    if (rockinfo->numbytes != offset) {
        fprintf(stderr, "Offset %d != rockInfo->numbytes %d\n", offset,
                rockinfo->numbytes);
    }

    /* fetch the file with volume data */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = CLIENTTOSERVER;
    sed.Value.SmartFTPD.ByteQuota             = -1;
    sed.Value.SmartFTPD.SeekOffset = -1; /* setting this to 'offset' wreaks
					    havoc with dumps > 4GB */
    sed.Value.SmartFTPD.hashmark   = 0;
    sed.Value.SmartFTPD.Tag        = FILEBYFD;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd = rockinfo->fd;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
        fprintf(stderr, "WriteDump: Error %s in InitSideEffect\n",
                RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
               RPC2_ELIMIT) {
        fprintf(stderr, "WriteDump: Error %s in CheckSideEffect\n",
                RPC2_ErrorMsg((int)rc));
    }

    if (sed.Value.SmartFTPD.BytesTransferred != (int)*nbytes) {
        fprintf(stderr, "Transmitted bytes %d != requested bytes %ld!\n",
                *nbytes, sed.Value.SmartFTPD.BytesTransferred);
        *nbytes = sed.Value.SmartFTPD.BytesTransferred;
    }
#if 0
    fprintf(stderr, "Transmitted %ld bytes.\n",
	    sed.Value.SmartFTPD.BytesTransferred);
#else
    fprintf(stderr, ".");
#endif
    rockinfo->numbytes += sed.Value.SmartFTPD.BytesTransferred;
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
    long rc        = 0;
    VolumeId volid = 0;
    char *filename = NULL;
    FILE *outf;

    while ((these_args > 2) && *this_argp[2] == '-') {
        if (strcmp(this_argp[2], "-f") == 0) {
            filename = this_argp[3];
            these_args--;
            this_argp++;
        }

        these_args--;
        this_argp++;
    }

    if (these_args < 3) {
        fprintf(
            stderr,
            "Usage: volutil restore [-f <file name>] <partition-name> [<volname> [<volid>]]\n");
        exit(EXIT_FAILURE);
    }

    partition = this_argp[2];
    stripslash(partition);

    if (these_args < 4)
        memset((void *)volname, 0, 70);
    else
        strcpy(volname, this_argp[3]);

    if ((these_args < 5) || (sscanf(this_argp[4], "%x", &volid) == 0))
        volid = 0;

    /* Create lwp thread DumpLwp */
    struct rockInfo *rock = (struct rockInfo *)malloc(sizeof(struct rockInfo));
    rock->volid           = volid;
    rock->numbytes        = 0;

    if (!filename)
        outf = stdin;
    else
        outf = fopen(filename, "r");

    rock->fd = fileno(outf);
    if (rock->fd < 0) {
        perror("RestoreFile");
        exit(EXIT_FAILURE);
    }

    PROCESS restorePid;
    LWP_CreateProcess(VolDumpLWP, 16 * 1024, LWP_NORMAL_PRIORITY, (void *)rock,
                      "VolDumpLWP", &restorePid);
    if (rc != LWP_SUCCESS) {
        fprintf(stderr, "VolDump can't create child %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }

    rc =
        VolRestore(rpcid, (RPC2_String)partition, (RPC2_String)volname, &volid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "\nVolRestore failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }

    printf("\nVolRestore successful, created %08x\n", volid);
    exit(EXIT_SUCCESS);
}

long S_ReadDump(RPC2_Handle rpcid, RPC2_Unsigned offset, RPC2_Integer *nbytes,
                VolumeId volid, SE_Descriptor *BD)
{
    long rc = 0;
    struct rockInfo *rockinfo;
    SE_Descriptor sed;
    char *rock;

    CODA_ASSERT(LWP_GetRock(ROCKTAG, &rock) == LWP_SUCCESS);
    rockinfo = (struct rockInfo *)rock;

    if (volid ==
        0) { /* User didn't assign one, use volId fileserver gives us. */
        rockinfo->volid = volid;
    }

    if (volid != rockinfo->volid) {
        fprintf(stderr, "Got a ReadDump for %08x, I'm reading %08x!\n", volid,
                rockinfo->volid);
        exit(EXIT_FAILURE);
    }

    CODA_ASSERT(rockinfo->fd != 0); /* Better have been opened by restore() */

    /* fetch the file with volume data */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.ByteQuota             = *nbytes;
    sed.Value.SmartFTPD.SeekOffset            = -1;
    sed.Value.SmartFTPD.hashmark              = 0;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = rockinfo->fd;

    if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
        fprintf(stderr, "ReadDump: Error %s in InitSideEffect\n",
                RPC2_ErrorMsg((int)rc));
    } else if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
               RPC2_ELIMIT) {
        fprintf(stderr, "ReadDump: Error %s in CheckSideEffect\n",
                RPC2_ErrorMsg((int)rc));
    }

#if 0
    fprintf(stderr, "Transmitted %ld bytes.\n", sed.Value.SmartFTPD.BytesTransferred);
#else
    fprintf(stderr, ".");
#endif
    rockinfo->numbytes += sed.Value.SmartFTPD.BytesTransferred;
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
        fprintf(stderr,
                "Usage: volutil dumpmem <address> <size> <file-name>\n");
        exit(EXIT_FAILURE);
    }
    int address, size;
    char *fname;

    sscanf(this_argp[2], "%x", &address);
    sscanf(this_argp[3], "%d", &size);
    fname = this_argp[4];

    rc = VolDumpMem(rpcid, (RPC2_String)fname, address, size);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolDumpMem failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Memory Dumped in file %s on server\n", fname);
    exit(EXIT_SUCCESS);
}

/**
 * rvmsize - display RVM statistics for a volume
 * @volumeid: volume replica id
 *
 * Print the RVM statistics for  the  volume  <volume-ID>.
 */
static void rvmsize(void)
{
    VolumeId volid;
    RVMSize_data data;

    if (these_args != 3) {
        fprintf(stderr, "Usage: volutil rvmsize <volid>\n");
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[2], "%x", &volid) != 1) {
        fprintf(stderr, "RVMSize: Bad Volumeid %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolRVMSize(rpcid, volid, &data);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolRVMSize failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Volume %08x used a total of %d bytes.\n", volid, data.VolumeSize);
    printf("\t%d small vnodes used %d bytes.\n", data.nSmallVnodes,
           data.SmallVnodeSize);
    printf("\t%d large vnodes used %d bytes.\n", data.nLargeVnodes,
           data.LargeVnodeSize);
    printf("\t and %d bytes of DirPages.\n", data.DirPagesSize);
    exit(EXIT_SUCCESS);
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
    VolumeId Vid, backupVid;

    if (these_args != 3) {
        fprintf(stderr, "Usage: volutil backup volumeId\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &Vid) != 1) {
        fprintf(stderr, "VolMakeBackups: Bogus volume number %s\n",
                this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolMakeBackups(rpcid, Vid, (VolumeId *)&backupVid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolMakeBackups failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Backup (id = %08x) of Volume %08x created\n", backupVid, Vid);
    exit(EXIT_SUCCESS);
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
    VolumeId volumeid = 0;
    long groupid;

    if (these_args != 5 && these_args != 6) {
        fprintf(
            stderr,
            "Usage: volutil create_rep partition volumename replicated-volid [replica-volid]\n");
        exit(EXIT_FAILURE);
    }
    partition = this_argp[2];
    stripslash(partition);
    volumeName = this_argp[3];

    if (sscanf(this_argp[4], "%lX", &groupid) != 1) {
        fprintf(stderr, "CreateRep: Bad Group Id %s\n", this_argp[4]);
        exit(EXIT_FAILURE);
    }
    if (these_args == 6) {
        if (sscanf(this_argp[5], "%x", &volumeid) != 1) {
            fprintf(stderr, "CreateRep: Bad Volume Id %s\n", this_argp[5]);
            exit(EXIT_FAILURE);
        }
    }

    rc = VolCreate(rpcid, (RPC2_String)partition, (RPC2_String)volumeName,
                   &volumeid, 1, groupid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolCreate failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Volume %08x (%s) created \n", volumeid, volumeName);
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }
    infile = this_argp[2];

    rc = VolMakeVLDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolMakeVLDB failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "VLDB completed.\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }
    infile = this_argp[2];

    rc = VolMakeVRDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolMakeVRDB failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "VRDB completed.\n");
    exit(EXIT_SUCCESS);
}

/**
 * dumpvrdb - dump the internal VRDB state into a new VRList file
 * @vrlist:	output list which will contains all information about volume
 *		replicas that is currently stored in RVM.
 */
static void dumpvrdb(void)
{
    char *infile;
    if (these_args != 3) {
        fprintf(stderr, "Usage: volutil dumpvrdb VRListFile\n");
        exit(EXIT_FAILURE);
    }
    infile = this_argp[2];

    rc = VolDumpVRDB(rpcid, (RPC2_String)infile);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolDumpVRDB failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
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
        } else {
            err = 1;
        }
        these_args--;
        this_argp++;
    }
    if (err || these_args < 3) {
        fprintf(stderr,
                "Usage: volutil info [-all] volumeName/volumeNumber [file]\n");
        exit(EXIT_FAILURE);
    }

    if (these_args < 4)
        outf = stdout;
    else
        outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = VolInfo(rpcid, (RPC2_String)this_argp[2], DumpAll, &sed);
    if (rc == -1) {
        fprintf(stderr, "VolInfo failed, %s not found\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolInfo failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="showvnode"><strong>Client end of the <tt>showvnode</tt> request</strong></a>
  END_HTML
*/
static void showvnode(void)
{
    SE_Descriptor sed;
    VolumeId volumeNumber;
    VnodeId vnodeNumber;
    Unique_t unique;
    FILE *outf;

    if (these_args < 5) {
        fprintf(
            stderr,
            "Usage: volutil showvnode volumeNumber vnodeNumber Unique [file]\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &volumeNumber) != 1) {
        fprintf(stderr, "showvnode: Bogus volume number %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[3], "%x", &vnodeNumber) != 1) {
        fprintf(stderr, "showvnode: Bogus vnode number %s\n", this_argp[3]);
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[4], "%x", &unique) != 1) {
        fprintf(stderr, "showvnode: Bogus Uniquifier %s\n", this_argp[4]);
        exit(EXIT_FAILURE);
    }

    if (these_args < 6)
        outf = stdout;
    else
        outf = fopen(this_argp[5], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = VolShowVnode(rpcid, volumeNumber, vnodeNumber, unique, &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolShowVnode failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
/*
  BEGIN_HTML
  <a name="setvv"><strong>Client end of the <tt>setvv</tt> request</strong></a>
  END_HTML
*/
static void setvv(void)
{
    VolumeId volumeNumber;
    VnodeId vnodeNumber;
    Unique_t unique;
    ViceVersionVector vv;

    if (these_args != 16) {
        fprintf(
            stderr,
            "Usage: volutil setvv volumeNumber vnodeNumber unique <version nubmers(8)> <StoreId (host) (Uniquifier)> <flags>\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &volumeNumber) != 1) {
        fprintf(stderr, "setvv: Bogus volume number %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[3], "%x", &vnodeNumber) != 1) {
        fprintf(stderr, "setvv: Bogus vnode number %s\n", this_argp[3]);
        exit(EXIT_FAILURE);
    }
    if (sscanf(this_argp[4], "%x", &unique) != 1) {
        fprintf(stderr, "setvv: Bogus vnode uniquifier %s\n", this_argp[4]);
        exit(EXIT_FAILURE);
    }
    memset((void *)&vv, 0, sizeof(vv));
    vv.Versions.Site0 = (bit32)atoi(this_argp[5]);
    vv.Versions.Site1 = (bit32)atoi(this_argp[6]);
    vv.Versions.Site2 = (bit32)atoi(this_argp[7]);
    vv.Versions.Site3 = (bit32)atoi(this_argp[8]);
    vv.Versions.Site4 = (bit32)atoi(this_argp[9]);
    vv.Versions.Site5 = (bit32)atoi(this_argp[10]);
    vv.Versions.Site6 = (bit32)atoi(this_argp[11]);
    vv.Versions.Site7 = (bit32)atoi(this_argp[12]);

    vv.StoreId.HostId     = atol(this_argp[13]);
    vv.StoreId.Uniquifier = atol(this_argp[14]);
    vv.Flags              = atol(this_argp[15]);

    rc = VolSetVV(rpcid, volumeNumber, vnodeNumber, unique, &vv);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSetVV failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "VolSetVV completed\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &volid) != 1) {
        fprintf(stderr, "Purge: Bad Volume Id %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolPurge(rpcid, volid, (RPC2_String)this_argp[3]);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolPurge failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Volume %08x (%s) successfully purged\n", volid, this_argp[3]);
    exit(EXIT_SUCCESS);
}
/*
  BEGIN_HTML
  <a name="lock"><strong>Client end of request to <tt>lock</tt> a volume</strong></a>
  END_HTML
*/
static void lock(void)
{
    VolumeId Vid;
    ViceVersionVector vvv;
    long rc;

    if (these_args != 3) {
        fprintf(stderr, "Usage: volutil lock volumeId\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &Vid) != 1) {
        fprintf(stderr, "VolLock: Bogus volume number %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolLock(rpcid, Vid, &vvv);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolLock failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Locked volume %08x had a VVV of (%d,%d,%d,%d,%d,%d,%d,%d)\n", Vid,
           vvv.Versions.Site0, vvv.Versions.Site1, vvv.Versions.Site2,
           vvv.Versions.Site3, vvv.Versions.Site4, vvv.Versions.Site5,
           vvv.Versions.Site6, vvv.Versions.Site7);
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="unlock"><strong>Client end of request to <tt>unlock</tt> a volume</strong></a>
  END_HTML
*/
static void unlock(void)
{
    VolumeId Vid;

    if (these_args != 3) {
        fprintf(stderr, "Usage: volutil unlock volumeId\n");
        exit(EXIT_FAILURE);
    }

    if (sscanf(this_argp[2], "%x", &Vid) != 1) {
        fprintf(stderr, "VolUnlock: Bogus volume number %s\n", this_argp[2]);
        exit(EXIT_FAILURE);
    }

    rc = VolUnlock(rpcid, Vid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolUnlock failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Volume %08x is unlocked.\n", Vid);
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (these_args < 4)
        outf = stdout;
    else
        outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = VolLookup(rpcid, (RPC2_String)this_argp[2], &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolLookup failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    rc = VolUpdateDB(rpcid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolUpdateDB failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Databases updated on host %s.\n", s_hostname);
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    rc = VolShutdown(rpcid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolShutdown failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Fileserver shutdown.\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    rc = VolSwaplog(rpcid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSwaplog failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Fileserver log successfully swapped.\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    rc = VolSwapmalloc(rpcid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSwapmalloc failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Malloc tracing successfuly toggled.\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }
    debuglevel = atoi(this_argp[2]);

    rc = VolSetDebug(rpcid, debuglevel);

    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolSetDebug failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "VolumeDebugLevel set to %d.\n", debuglevel);
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (strcmp(this_argp[2], "on") == 0)
        on = 1;
    else if (strcmp(this_argp[2], "off") == 0)
        on = 0;

    if (on == -1) {
        fprintf(stderr, "Usage: volutil timing <on | off>\n");
        exit(EXIT_FAILURE);
    }

    if (these_args < 4)
        outf = stdout;
    else
        outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = VolTiming(rpcid, on, &sed);

    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "VolTiming failed with return code %ld\n", rc);
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Timing finished successfully\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (these_args < 4)
        outf = stdout;
    else
        outf = fopen(this_argp[3], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = TraceRpc(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "TraceRpc failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "TraceRpc finished successfully\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (these_args < 3)
        outf = stdout;
    else
        outf = fopen(this_argp[2], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = PrintStats(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "PrintStats failed with %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "PrintStats finished successfully\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    if (these_args < 3)
        outf = stdout;
    else
        outf = fopen(this_argp[2], "w");

    /* set up side effect descriptor */
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = GetVolumeList(rpcid, &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "GetVolumeList failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "GetVolumeList finished successfully\n");
    exit(EXIT_SUCCESS);
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
        fprintf(
            stderr,
            "Usage: volutil showcallbacks <volumeid> <vnode> <unique> [file]\n");
        exit(EXIT_FAILURE);
    }

    if ((sscanf(this_argp[2], "%x", &fid.Volume) != 1) ||
        (sscanf(this_argp[3], "%x", &fid.Vnode) != 1) ||
        (sscanf(this_argp[4], "%x", &fid.Unique) != 1)) {
        fprintf(
            stderr,
            "Usage: volutil showcallbacks <volumeid> <vnode> <unique> <out-file>\n");
        exit(EXIT_FAILURE);
    }

    if (these_args < 6)
        outf = stdout;
    else
        outf = fopen(this_argp[5], "w");

    /* set up side effect descriptor */
    SE_Descriptor sed;
    memset(&sed, 0, sizeof(SE_Descriptor));
    sed.Tag                                   = SMARTFTP;
    sed.Value.SmartFTPD.Tag                   = FILEBYFD;
    sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
    sed.Value.SmartFTPD.FileInfo.ByFD.fd      = fileno(outf);

    rc = ShowCallbacks(rpcid, &fid, &sed);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "Showcallbacks failed with %s\n",
                RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Showcallbacks finished successfully\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }
    fprintf(
        stderr,
        "Truncate of RVM log started....Wait for a few minutes for it to complete\n");
    exit(EXIT_SUCCESS);
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
        exit(EXIT_FAILURE);
    }

    rc = VolGetMaxVolId(rpcid, (RPC2_Integer *)&maxid);
    if (rc != RPC2_SUCCESS) {
        fprintf(stderr, "Couldn't get maxvolid: %s\n", RPC2_ErrorMsg((int)rc));
        exit(EXIT_FAILURE);
    }
    printf("Maximum volume id is %08x\n", maxid);
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="setmaxvol"><strong>Client end of the setmaxvol request</strong></a>
  END_HTML
*/
static void setmaxvol(void)
{
    VolumeId volid;

    if ((these_args != 3) || (sscanf(this_argp[2], "%x", &volid) != 1)) {
        fprintf(stderr, "Usage: volutil setmaxvol <volumeid>\n");
        exit(EXIT_FAILURE);
    }

    rc = VolSetMaxVolId(rpcid, volid);
    if (rc != RPC2_SUCCESS) {
        fprintf(
            stderr,
            "Couldn't set new maximum volume id, check that you didn't try\n"
            "to change the server id or set a new maxid that is less than\n"
            "the current maximum id.\n");
        exit(EXIT_FAILURE);
    }

    printf("Maximum volume id set to %08x\n", volid);
    exit(EXIT_SUCCESS);
}

static void peekpokeerr(void)
{
    static const char *msgs[] = {
        /*0*/ "Path to server file not known",
        /*1*/ "Cannot read symbols from the server file",
        /*2*/ "Symbol not found in the server file",
        /*3*/ "Address not in server virtual space",
        /*4*/ "Memory protection error",
        /*5*/ "Unaligned integer/pointer reference"
    };
    fprintf(stderr, "Couldn't %s at %s: %s\n", this_argp[1], this_argp[2],
            (int)rc >= -10040L &&
                    rc < (int)(sizeof(msgs) / sizeof(*msgs) - 10040L) ?
                msgs[10040 + (int)rc] :
                RPC2_ErrorMsg((int)rc));
    exit(EXIT_FAILURE);
}

static void usageerr(const char *args)
{
    fprintf(stderr, "Usage: %s %s %s\n", this_argp[0], this_argp[1], args);
    exit(EXIT_FAILURE);
}

static int sscani(char *s, RPC2_Unsigned *px)
{
    RPC2_Unsigned k;
    int q;

    while (isspace(*s))
        s++;

    if (*s != '0')
        q = sscanf(s, "%u", &k);
    else if (*++s != 'x')
        q = sscanf(s, "%o", &k);
    else
        q = sscanf(++s, "%x", &k);

    *px = k;
    return (q);
}

/*
  BEGIN_HTML
  <a name="peekint"><strong>Client end of the peek request</strong></a>
  END_HTML
*/
static void peekint(void)
{
    RPC2_Integer value;

    if (these_args != 3)
        usageerr("<address>");

    if ((rc = VolPeekInt(rpcid, (RPC2_String)this_argp[2], &value)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    printf("%s contains %x\n", this_argp[2], value);
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="pokeint"><strong>Client end of the poke request</strong></a>
  END_HTML
*/
static void pokeint(void)
{
    RPC2_Unsigned value;

    if ((these_args != 4) || (sscani(this_argp[3], &value) != 1))
        usageerr("<address> <value>");

    if ((rc = VolPokeInt(rpcid, (RPC2_String)this_argp[2], value)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    fprintf(stderr, "%x stored at %s\n", value, this_argp[2]);
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="peekmem"><strong>Client end of the <tt>peeks()</tt> request</strong></a>
  END_HTML
*/
static void peekmem(void)
{
    RPC2_BoundedBS buf;
    RPC2_Unsigned tmp;

    if ((these_args != 4) || (sscani(this_argp[3], &tmp) != 1))
        usageerr("<address> <size>");
    buf.MaxSeqLen = tmp;

    if ((buf.SeqBody = (RPC2_String)malloc((int)buf.MaxSeqLen + 1)) == NULL) {
        fprintf(stderr, "volutil: Out of memory\n");
        exit(EXIT_FAILURE);
    }
    buf.SeqLen = 0;

    if ((rc = VolPeekMem(rpcid, (RPC2_String)this_argp[2], &buf)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    buf.SeqBody[(int)buf.SeqLen] = '\0';
    printf("%s contains %s\n", this_argp[2], buf.SeqBody);
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="pokemem"><strong>Client end of the <tt>pokes()</tt> request</strong></a>
  END_HTML
*/
static void pokemem(void)
{
    RPC2_CountedBS buf;
    RPC2_Unsigned tmp;

    if ((these_args != 5) || (sscani(this_argp[3], &tmp) != 1))
        usageerr("<address> <size> <value>");
    buf.SeqLen  = tmp;
    buf.SeqBody = (RPC2_String)this_argp[4];

    if ((rc = VolPokeMem(rpcid, (RPC2_String)this_argp[2], &buf)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    printf("%s stored at %s\n", buf.SeqBody, this_argp[2]);

    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="peekxmem"><strong>Client end of the <tt>peekx()</tt> request</strong></a>
  END_HTML
*/
static void peekxmem(void)
{
    RPC2_BoundedBS buf;
    RPC2_Unsigned tmp;

    if ((these_args != 4) || (sscani(this_argp[3], &tmp) != 1))
        usageerr("<address> <size>");
    buf.MaxSeqLen = tmp;

    if ((buf.SeqBody = (RPC2_String)malloc((int)buf.MaxSeqLen)) == NULL) {
        fprintf(stderr, "volutil: Out of memory\n");
        exit(EXIT_FAILURE);
    }
    buf.SeqLen = 0;

    if ((rc = VolPeekMem(rpcid, (RPC2_String)this_argp[2], &buf)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    printf("%s contains 0x", this_argp[2]);
    while (buf.SeqLen--)
        printf("%02x", *buf.SeqBody++);
    printf("\n");
    exit(EXIT_SUCCESS);
}

/*
  BEGIN_HTML
  <a name="pokexmem"><strong>Client end of the <tt>pokex()</tt> request</strong></a>
  END_HTML
*/
static void pokexmem(void)
{
    RPC2_CountedBS buf;
    char *t, *s;
    RPC2_Integer size;
    RPC2_Unsigned tmp;

    if ((these_args != 5) || (sscani(this_argp[3], &tmp) != 1))
        usageerr("<address> <size> <hexvalue>");
    buf.SeqLen = tmp;

    if ((buf.SeqBody = (RPC2_String)malloc((int)buf.SeqLen)) == NULL) {
        fprintf(stderr, "volutil: Out of memory\n");
        exit(EXIT_FAILURE);
    }
    if ((s = this_argp[4])[0] == '0' && s[1] == 'x')
        s += 2;
    t    = (char *)buf.SeqBody;
    size = buf.SeqLen;
    while (size--) {
        int vh, vl;
        if (!isxdigit(s[0]) || !isxdigit(s[1])) {
            fprintf(stderr, "%s is not a %s-byte hex string\n", this_argp[4],
                    this_argp[3]);
            usageerr("<address> <size> <hexvalue>");
        }
        vh = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
        s++;
        vl = *s - (isdigit(*s) ? '0' : (islower(*s) ? 'a' - 10 : 'A' - 10));
        s++;
        *t++ = (vh << 4) + vl;
    }

    if ((rc = VolPokeMem(rpcid, (RPC2_String)this_argp[2], &buf)) !=
        RPC2_SUCCESS)
        peekpokeerr();
    printf("0x%s stored at %s\n",
           (this_argp[4][0] == '0' && this_argp[4][1] == 'x') ?
               this_argp[4] + 2 :
               this_argp[4],
           this_argp[2]);
    exit(EXIT_SUCCESS);
}

static void V_InitRPC(int timeout)
{
    PROCESS mylpid;
    SFTP_Initializer sftpi;
    RPC2_Options options;
    struct timeval tout;
    long rcode;

    CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_MAX_PRIORITY - 1, &mylpid) ==
                LWP_SUCCESS);

    SFTP_SetDefaults(&sftpi);
    SFTP_Activate(&sftpi);
    tout.tv_sec  = timeout;
    tout.tv_usec = 0;

    memset(&options, 0, sizeof(options));
    options.Flags = RPC2_OPTION_IPV6;

    rcode = RPC2_Init(RPC2_VERSION, &options, NULL, 5, &tout);
    if (rcode != RPC2_SUCCESS) {
        fprintf(stderr, "RPC2_Init failed with %s\n",
                RPC2_ErrorMsg((int)rcode));
        exit(EXIT_FAILURE);
    }
}

static int V_BindToServer(char *fileserver, char *realm, RPC2_Handle *RPCid)
{
    static struct secret_state state;
    /* Binds to File Server on volume utility port on behalf of uName.
    Sets RPCid to the value of the connection id.    */

    RPC2_HostIdent hident;
    RPC2_PortIdent pident;
    RPC2_SubsysIdent sident;
    RPC2_EncryptionKey secret;
    ClearToken ctok;
    EncryptedSecretToken stok;
    RPC2_CountedBS clientident;
    long rcode;
    struct servent *s = coda_getservbyname("codasrv", "udp");

    hident.Tag = RPC2_HOSTBYNAME;
    strcpy(hident.Value.Name, fileserver);

    pident.Tag                  = RPC2_PORTBYINETNUMBER;
    pident.Value.InetPortNumber = s->s_port;

    sident.Tag            = RPC2_SUBSYSBYID;
    sident.Value.SubsysId = UTIL_SUBSYSID;

    fprintf(stderr, "V_BindToServer: binding to host %s\n", fileserver);
    RPC2_BindParms bparms;
    memset((void *)&bparms, 0, sizeof(bparms));
    bparms.SecurityLevel  = RPC2_AUTHONLY;
    bparms.EncryptionType = RPC2_XOR;
    bparms.SideEffectType = SMARTFTP;

    if (GetSecret(vice_config_path(VolTKFile), secret, &state) == 0) {
        bparms.AuthenticationType = AUTH_METHOD_VICEKEY;
        bparms.SharedSecret       = &secret;
    } else if (realm && U_GetLocalTokens(&ctok, stok, realm) == 0) {
        clientident.SeqLen  = sizeof(SecretToken);
        clientident.SeqBody = (RPC2_ByteSeq)&stok;

        bparms.AuthenticationType = AUTH_METHOD_CODATOKENS;
        bparms.ClientIdent        = &clientident;
        bparms.SharedSecret       = &ctok.HandShakeKey;
    } else {
        printf("Couldn't get a token to authenticate with the server\n");
        exit(EXIT_FAILURE);
    }

    rcode = RPC2_NewBinding(&hident, &pident, &sident, &bparms, RPCid);
    if (rcode < 0 && rcode > RPC2_ELIMIT)
        rcode = 0;
    if (rcode == 0 || rcode == RPC2_NOTAUTHENTICATED)
        return (rcode);
    else {
        fprintf(stderr, "RPC2_NewBinding to server %s failed with %s\n",
                fileserver, RPC2_ErrorMsg((int)rcode));
        exit(EXIT_FAILURE);
    }
}
