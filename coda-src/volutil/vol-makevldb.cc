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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/volutil/vol-makevldb.cc,v 4.7 1998/11/02 16:47:12 rvb Exp $";
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

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <voltypes.h>
#include <voldefs.h>
#include <vldb.h>
#include <vice.h>
#include <srv.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>
#include <vutil.h>

/* ReadWrite volumes:   are stored exactly twice in the data base.
			Once under name of volume, once under number.
			Only one server site will be recorded.
			Latest read-only and backup-volumes corresponding to
			the read-write volume will be recorded in the read
			write entry.

   Backup volumes:	Only the latest backup volume is stored by name
   			in the data base; it is also stored by number as are
			any old backup volumes which may still exist or appear
			to exist.  The corresponding read-write and read-only
			volumes will not be recorded.

   Readonly volumes:	Same as backup volumes.
			
 */

/* static int debug = 0; */

static char *ServerName[256];
static char *args[50]={NULL};
static struct vldb *vldb_array;/* In core copy of vldb */
static long *Dates;	  /* copy date for the volume at this slot in vldb */
static struct vldb **RWindex; /* index of named rw entry */
static int vldbSize;	  /* array size, in elements */
static int vldbHashSize; /* Hash index space (1 to vldbHashSize) */
static int haveEntry = 0;
static int MaxStride;

#define vldbindex(p)	((p) - &vldb_array[0])

#define RWLIST_PATH	"/vice/vol/RWlist"
#define PART_PATH	"/vice/vol/partitions"
#define PART_TEMP	"/vice/vol/partitions.new"
#define ALL_COMMAND	"sort >/vice/vol/AllVolumes.new"
#define ALL_TEMP	"/vice/vol/AllVolumes.new"
#define ALL_PATH	"/vice/vol/AllVolumes"

static FILE *BackupList, *RWList, *PartList, *AllList, *volumelist;

static int Pass(char type);
static void InitAddEntry();
static void VolumeEntry(char type, int byname, char *name,
		unsigned long volume, int server, unsigned long readwrite,
		int creationdate, int copydate, int backupdate);
static void AddReadWriteEntry(struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate);
static void AddReadOnlyEntry(struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate);
static void AddBackupEntry(struct vldb *vnew, int byname,
		char *name, unsigned long volume, int server,
		unsigned long readwrite, int creationdate,
		int copydate, int backupdate);
static struct vldb *Lookup(char *key, long *date);
static void Add(struct vldb *vnew, long date);
static void Replace(struct vldb *old, struct vldb *vnew, long date);
static void AddServer(struct vldb *old, struct vldb *vnew);
static void AddAssociate(struct vldb *old, struct vldb *vnew);
static void CheckRWindex(unsigned long volume, char *name);
static void GetPartitionInfo(char **argp, int nargs);
static void GetArgs(char *line, char **args, int *nargs);
static char *AllocString(char *s);
static void GetServerNames();

/* debugging hack: */
char rootname[32];


/*
  BEGIN_HTML
  <a name="S_VolMakeVLDB"><strong>Rebuild the VLDB from the file listed as parameter</strong></a> 
  END_HTML
*/
long S_VolMakeVLDB(RPC2_Handle rpcid, RPC2_String formal_infile) {
    int nentries;
    struct vldbHeader *head;
    int fd;
    int MaxRO, MaxBK, MaxRW;
    int err = 0;
    ProgramType *pt;
    struct vldb *tmpvldb;

    /* To keep C++ 2.0 happy */
    char *infile = (char *)formal_infile;

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolMakeVLDB; infile %s", infile);
    GetServerNames();
    InitAddEntry();

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    volumelist = fopen(infile, "r");
    if (volumelist == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeVLDB: unable to open file %s", infile);
	return(VNOVNODE);
    }
    PartList = fopen(PART_TEMP, "w");
    nentries = 2*Pass('P');	/* Count lines & get partition info;
    				   have roughly twice as many data base
				   entries as lines
				   since each is entered under two keys */
    vldbHashSize = nentries+(nentries/2);
    /* allow for overflow of high hash values */
    vldbSize = vldbHashSize + 100;
    vldb_array = (struct vldb *) calloc(vldbSize,  sizeof(struct vldb));
    Dates = (long *) calloc(vldbSize, sizeof (*Dates));
    RWindex = (struct vldb **) calloc(vldbSize, sizeof(*RWindex));

    BackupList = fopen(BACKUPLIST_PATH, "w");
    if (BackupList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", BACKUPLIST_PATH);
	return(VNOVNODE);
    }

    RWList = fopen(RWLIST_PATH, "w");
    if (RWList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", RWLIST_PATH);
	return(VNOVNODE);
    }

#ifdef __CYGWIN32__
    AllList = fopen(ALL_TEMP, "w");
    if (AllList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", ALL_TEMP);
  	return(VFAIL);
    }
#else
    AllList = (FILE *) popen(ALL_COMMAND, "w");
    if (AllList == NULL) {
	printf("makevldb:  Unable to \"run %s\"; aborted\n", ALL_COMMAND);
  	return(VFAIL);
    }
#endif

    Pass('W');	/* Read-write volumes */
    MaxRW = MaxStride;
    Pass('R');	/* Read only (cloned) volumes */
    MaxRO = MaxStride;
    Pass('B');	/* Backup volumes */
    MaxBK = MaxStride;
    head = (struct vldbHeader *) vldb_array;
    head->magic = htonl(VLDB_MAGIC);
    head->hashSize = htonl(vldbHashSize);
    
    fclose(volumelist);
    fclose(BackupList);
    fclose(RWList);

    if (haveEntry == 0) {
	printf("makevldb:  no data base entries found in input file; aborted\n");
	return(VNOVNODE);
    }
    fd = open(VLDB_TEMP, O_TRUNC|O_WRONLY|O_CREAT, 0644);
    if (fd == -1) {
	printf("makevldb:  Unable to create %s; aborted\n", VLDB_TEMP);
	return(VNOVNODE);
    }
    if (write(fd, (char *)vldb_array, vldbSize*sizeof(struct vldb)) != vldbSize*sizeof(struct vldb)) {
	perror("makevldb");
	unlink(VLDB_TEMP);
	return(VNOVNODE);
    }
    close(fd);
    if (rename(VLDB_TEMP, VLDB_PATH) == -1) {
	printf("Unable to rename %s to %s; new vldb not created\n", VLDB_TEMP, VLDB_PATH);
	err = 1;
    }
    else
        printf("VLDB created.  Search lengths: RO %d, RW %d, BK %d.\n", MaxRO, MaxRW, MaxBK);

    fclose(PartList);
    if (rename(PART_TEMP, PART_PATH) == -1) {
	printf("Unable to rename %s to %s; new partition list not created\n", PART_TEMP, PART_PATH);
	err = 1;
    }

    fprintf(AllList, "# [Last line of unsorted volume list]\n");
#ifdef __CYGWIN32__
    fclose(AllList);
#else
    pclose(AllList);
#endif
    if (rename(ALL_TEMP, ALL_PATH) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to rename %s to %s; new volume list not created", ALL_TEMP, ALL_PATH);
	err = 1;
    }
    else
	LogMsg(0, VolDebugLevel, stdout, "%s written", ALL_PATH);

    /* tell fileserver to read in new database */
    VCheckVLDB();

    return(err?VFAIL:RPC2_SUCCESS);
}

static int Pass(char type)
{
    char *name;
    unsigned long volume;
    int server;
    char *partition;
    int minquota;
    int maxquota;
    int diskusage;
    unsigned long readwrite;
    int copydate, backupdate, creationdate;
    int volumeusage = 0; /* Some sort of metric for recent volume usage */
    char line[500];
    char idname[20]; 
    int nargs;
    int linenumber = 0;

    LogMsg(29, VolDebugLevel, stdout, "Entering Pass %c", type);
    MaxStride = 0;
    rewind(volumelist);
    while (fgets(line, sizeof(line), volumelist) != NULL) {
	linenumber++;
	if (line[0] == type) {
	    register char **argp = args;
	    GetArgs(line,argp,&nargs);
	    name = &argp[0][1];
	    if (type == 'P')
		GetPartitionInfo(argp, nargs);
	    else {
		while (++argp,--nargs) {
		    switch (**argp) {
			case 'I':
			    sscanf(&(*argp)[1], "%x", &volume);
			    break;
			case 'H':
			    sscanf(&(*argp)[1], "%x", &server);
			    break;
			case 'P':
			    partition = &(*argp)[1];
			    break;
			case 'm':
			    sscanf(&(*argp)[1], "%x", &minquota);
			    break;
			case 'M':
			    sscanf(&(*argp)[1], "%x", &maxquota);
			    break;
			case 'U':
			    sscanf(&(*argp)[1], "%x", &diskusage);
			    break;
			case 'W':
			    sscanf(&(*argp)[1], "%x", &readwrite);
			    break;
			case 'D':
			    sscanf(&(*argp)[1], "%x", &copydate);
			    break;
			case 'B':
			    sscanf(&(*argp)[1], "%x", &backupdate);
			    break;
			case 'C':
			    sscanf(&(*argp)[1], "%x", &creationdate);
			    break;
			case 'A':
			    sscanf(&(*argp)[1], "%x", &volumeusage);
			    break;
			default:
			    LogMsg(0, VolDebugLevel, stdout, "Bad input field, line %d, field \"%s\": VolMakeVLDB aborted",
				linenumber, *argp);
			    return(VFAIL);
		    }
		}
		sprintf(idname, "%u", volume);
		/* debugging hack: */
		sprintf(rootname, "%u", volume);
		VolumeEntry(type, 0, idname, volume, server, readwrite, creationdate, copydate, backupdate);
		VolumeEntry(type, 1, name, volume, server, readwrite, creationdate, copydate, backupdate);
		fprintf(AllList, "%s %x %s %s %u %u %u %c %u %u %u\n", name, volume, ServerName[server],
			    partition, diskusage, minquota, maxquota, type, creationdate, copydate, volumeusage);
		if (type == 'B') {
		    register char *dot;
		    if (dot = (char *) rindex(name, '.'))
			*dot = 0;
		    fprintf(BackupList, "%s %x %s %s\n", name, readwrite, ServerName[server], partition);
		}
		else if (type == 'W') {
		    fprintf(RWList, "%s %x %s %s %u %u %u\n", name, volume, ServerName[server],
			    partition, diskusage, minquota, maxquota);
		}
	    }
	}
    }
    return linenumber;
}

/*int (*AddEntry[MAXVOLTYPES])();*/
int (*AddEntry[MAXVOLTYPES])(...);

static void InitAddEntry() {
    AddEntry[backupVolume] = (int (*)(...))AddBackupEntry;
    AddEntry[readonlyVolume] = (int (*)(...))AddReadOnlyEntry;
    AddEntry[readwriteVolume] = (int (*)(...))AddReadWriteEntry;
}

static void VolumeEntry(char type, int byname, char *name, unsigned long volume,
		int server, unsigned long readwrite, int creationdate,
		int copydate, int backupdate)
{
    struct vldb vnew;
    int takenew = 0;
    haveEntry = 1;
    bzero((char *)&vnew, sizeof(vnew));
    strncpy(vnew.key, name, sizeof(vnew.key)-1);
    vnew.hashNext = 0;
    vnew.volumeType = (type=='B' ? backupVolume : type=='R' ? readonlyVolume : readwriteVolume);
    vnew.nServers = 1;
    vnew.volumeId[readwriteVolume] = htonl(readwrite);
    vnew.volumeId[vnew.volumeType] = htonl(volume);
    vnew.serverNumber[0] = server;
    (*AddEntry[vnew.volumeType])(&vnew, byname, name, volume, server, readwrite, creationdate, copydate, backupdate);
}



static void AddReadWriteEntry(register struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate)
{
    register struct vldb *old;
    long olddate;
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType != readwriteVolume || copydate > olddate)
	    Replace(old, vnew, copydate);
    }
    else {
	Add(vnew, copydate);
    }
    if (byname)
	CheckRWindex(volume, name);
}

static void AddReadOnlyEntry(register struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate)
{
    long olddate;
    register struct vldb *old;
    int added = 0;
    char rwname[100];
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType == readonlyVolume) {
	    if (creationdate > olddate) {
		Replace(old, vnew, creationdate);
		added = 1;
	    }
	    else if (creationdate == olddate) {
		AddServer(old, vnew);
	    }
	}
    }
    else {
	Add(vnew, creationdate);
	added = 1;
    }
    sprintf(rwname, "%u", readwrite);
    if (byname && added) {
        register struct vldb *rwp;
	rwp = Lookup(rwname, &olddate);
        if (rwp) {
	    AddAssociate(rwp, vnew);
	    rwp = RWindex[vldbindex(rwp)];
	    if (rwp)
	        AddAssociate(rwp, vnew);
	}
    }
}

static void AddBackupEntry(register struct vldb *vnew, int byname,
		char *name, unsigned long volume, int server,
		unsigned long readwrite, int creationdate,
		int copydate, int backupdate)
{
    long olddate;
    register struct vldb *old;
    char rwname[100];
    int added = 0;
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType == backupVolume) {
	    if (backupdate > olddate) {
		Replace(old, vnew, backupdate);
		added = 1;
	    }
	}
    }
    else {
	Add(vnew, backupdate);
	added = 1;
    }
    sprintf(rwname, "%u", readwrite);
    if (byname && added) {
        register struct vldb *rwp;
	rwp = Lookup(rwname, &olddate);
        if (rwp) {
	    AddAssociate(rwp, vnew);
	    rwp = RWindex[vldbindex(rwp)];
	    if (rwp)
	        AddAssociate(rwp, vnew);
	}
    }
}

static struct vldb *Lookup(char *key, long *date)
{
    register struct vldb *p;
    int index = HashString(key, vldbHashSize);
    for(p = &vldb_array[index]; p->key[0]; p += p->hashNext) {
	if (strcmp(p->key, key) == 0) {
	    *date = Dates[vldbindex(p)];
	    return p;
	}
	if (p->hashNext == 0)
	    break;
    }
    return 0;
}

static void Add(register struct vldb *vnew, long date)
{
    register struct vldb *p, *prev, *first;
    int index = HashString(vnew->key, vldbHashSize);
    LogMsg(19, VolDebugLevel, stdout, "Adding VLDB Entry for vol with key = %s", vnew->key);
    LogMsg(19, VolDebugLevel, stdout, "Add: index = %d, hashsize = %d", index, vldbHashSize);
    for(first = p = &vldb_array[index]; p->hashNext; p += p->hashNext)
        ;
    prev = p;
    while (p->key[0])
        p++;
    *p = *vnew;
    prev->hashNext = p - prev;
    if (MaxStride < p-first)
        MaxStride = p-first;
    Dates[vldbindex(p)] = date;
    return;
}

static void Replace(register struct vldb *old, register struct vldb *vnew,
							long date)
{
    vnew->hashNext = old->hashNext;
    *old = *vnew;
    Dates[vldbindex(old)] = date;
}

static void AddServer(register struct vldb *old, register struct vldb *vnew)
{
    register int i;
    for (i = 0; i<old->nServers; i++) {
	if (vnew->serverNumber[0] == old->serverNumber[i])
	    return;
    }
    old->serverNumber[old->nServers++] = vnew->serverNumber[0];
}

static void AddAssociate(register struct vldb *old, register struct vldb *vnew)
{
    old->volumeId[vnew->volumeType] = vnew->volumeId[vnew->volumeType];
}

static void CheckRWindex(unsigned long volume, char *name)
{
    long date;
    register struct vldb *idp, *namep;
    char idname[100];
    sprintf(idname, "%u", volume);
    idp = Lookup(idname, &date);
    namep = Lookup(name, &date);
    if (idp == 0 || namep == 0)
        return;
    if (idp->volumeType != readwriteVolume || namep->volumeType != readwriteVolume)
	return;
    RWindex[vldbindex(idp)] = namep;
}

static void GetPartitionInfo(char **argp, int nargs)
{
    char *partition;
    char *server = 0;
    unsigned int total, free;
    partition = &argp[0][1];
    while (++argp, --nargs) {
	switch (**argp) {
	    case 'H':
		server = &argp[0][1];
		break;
	    case 'T':
	        sscanf(&(*argp)[1], "%x", &total);
		break;
	    case 'F':
	        sscanf(&(*argp)[1], "%x", &free);
		break;
	}
    }
    fprintf(PartList, "%s %s %u %u\n", server, partition, total, free);
}

static void GetArgs(register char *line,register char **args,
						register int *nargs)
{
    *nargs = 0;
    while (*line) {
	register char *last = line;
	while (*line == ' ')
	    line++;
	if (*last == ' ')
	    *last = 0;
	if (!*line)
	    break;
	*args++  = line, (*nargs)++;
	while (*line && *line != ' ')
	    line++;
    }
}

static char *AllocString(char *s)
{
    char *as = (char *) malloc(strlen(s)+1);
    strcpy(as, s);
    return(as);
}

static void GetServerNames() {
    char line[200];
    char *serverList = SERVERLISTPATH;
    FILE *file;
    file = fopen(serverList, "r");
    if (file == NULL) {
	printf("Unable to read file %s; aborted\n", serverList);
	exit(1);
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char sname[50];
	struct hostent *hostent;
	long sid;
	if (sscanf(line, "%s%d", sname, &sid) == 2) {
	    if (sid > N_SERVERIDS) {
		printf("Warning: host %s is assigned a bogus server number (%u) in %s; host ignored\n",
		  sname, sid, serverList);
		continue;
	    }
	    hostent = gethostbyname(sname);
	    if (hostent == NULL)
		printf("Warning: host %s (listed in %s) is not in /etc/hosts\n", sname, serverList);
	    else
		ServerName[sid] = AllocString(hostent->h_name);
	}
    }
    fclose(file);
}

