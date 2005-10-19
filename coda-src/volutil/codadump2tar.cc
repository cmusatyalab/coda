/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2004 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


/* codadump2tar: takes Coda dump file as input and  produces a 
   standard tar file as output.  Enables Coda volumes to be backed up 
   so that they can be restored anywhere, even without use of Coda.

   Intended to work correctly even with full Unix semantics (e.g. even
   when hard links across directories are allowed); cannot be tested for
   those corner cases yet because Coda doesn't support them today.

   Note: this code makes no assumptions about ordering of large vnodes
   or small vnodes within their respective regions of the input dump file.

   Created: Satya, May 2004
*/


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <stdio.h>
#include <parser.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <lwp/lwp.h>
#include <lwp/lock.h>

#include "olist.h"
#include "ohash.h"


#include "tar-FromRedHatCD.h" 
  /* replace above copy of tar.h from RedHat tar source in volutil directory 
     by proper tar.h in /usr/include soon; right now, /usr/include/tar.h 
     seems to be a very different file (Satya May 2004) */

#ifdef __cplusplus
}
#endif

#include <util.h>
#include <voltypes.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#include <codadir.h>
#include "dump.h"
#include "dumpstream.h"


/* ------------ Global type definitions ------------ */

/* volume is fixed in dump; so (vnode,uniquifier) pair identifies 
   object;  note that the Coda directory package uses these in 
   network order,  so we need to be careful about making conversions 
   as needed; all the code below is kept in host order for sanity */
typedef struct {
  VnodeId vnode;
  Unique_t uniquifier;
} objectid_t;

/* entry in hash table for one object in dump */
class DumpObject:public olink {
public:
  objectid_t oid;

  int links;
  DumpObject **parents;
  char **components;

  /* the following are only used by directories */
  unsigned int isdir;
  unsigned int children;
  unsigned int dir_mtime;

  DumpObject(VnodeId, Unique_t);
  void AddParent(DumpObject *parent, const char *component);
  size_t GetFullPath(char *buf, size_t len, int idx=0); /* full path */
  size_t GetComponent(char *buf, size_t len, int idx); /* last path element */
  size_t GetPrefix(char *buf, size_t len, int idx); /* full path of our parent */
};

/* tar record and associated routines */

class TarRecd {
  union block tarblock;  /* the actual tar record */

public:
  /* fill these fields before calling Format();
     lengths of name fields below match those in tar.h
  */
  char tr_type;
  unsigned int tr_mode;
  unsigned int tr_uid;
  unsigned int tr_size;
  unsigned int tr_mtime;
  char tr_name[100];
  char tr_linkname[100];
  char tr_prefix[155];

  TarRecd(); /* constructor */
  void Reset(); /* reinits all field */
  void GetNameParts(DumpObject *obj, int idx=0);  /* extracts name fields */
  void Format();   /* fills tarblock */
  void Output(); /* writes tarblock to output */
  void WriteZeroTrailer(); /* write two blocks of zero records  */
};


/* ------------ Global variable definitions ------------ */

/* from "-f xxx" on command line; default of zero implies stdin */
char DumpFileName[MAXPATHLEN];  
dumpstream *DStream;     /* open stream */

/* from "-o xxx" on command line; default of zero means STDOUT */
char TarFileName[MAXPATHLEN]; 
FILE *TarFile = stdout;  /* open file handle for output */

/* from "-rn xxx" on command line; if not specified, uses 
   volume name from dump */
char *RootName;

int DebugLevel = 0; /* set by "-d xxx" command line switch */
#define DEBUG_HEAVY  (DebugLevel >= 10)
#define DEBUG_LIGHT  (DebugLevel > 0 && DebugLevel < 10)

/* Hash table of all objects found in dump */
ohashtab *DumpTable;

/* stuff read from dump */
struct DumpHeader ThisHead;    /* info about this dump */
VolumeDiskData ThisVDD;       /* info pertaining to entire volume */

/* Array of all directory vnodes */
DumpObject **LVNlist; /* malloc'ed when no. of large vnodes known */
int  LVNfillcount; /* no of entries filled in LVNlist */


/* TarObj is used to create and write out tar records */
TarRecd TarObj;


#define DEFAULT_HASHPOWER 12 /* 2^HashPower is # of buckets in hash table */
int HashPower = DEFAULT_HASHPOWER; /* can be set by "-hp x" on command line */


/* ----- Forward refs to non-method procs defined later in this file ----- */

/* steps of main() */
void ParseArgs(int, char **);
void DoGlobalSetup();
int ProcessDirectory();
void DiscoverPathnames();
void VerifyEverythingNamed();
void CreateEmptyDirectories();
int ProcessFileOrSymlink();
void ProcessHardLinks();

/* helper routines */
DumpObject *GetDumpObj(VnodeId, Unique_t);
int LowBits(DumpObject *);
void FreeDirectory(PDirInode);
int AddNameEntry(struct DirEntry *, void *);
void CreateHardLinkRecd(DumpObject *, int idx);

/* -------------------------------------------------- */

int main(int argc, char **argv)
{
  int i, rc;
  long vLargeCount, vLargeSize;
  long vSmallCount, vSmallSize;

  /* initialize globals */
  memset(DumpFileName, 0, sizeof(DumpFileName));
  memset(TarFileName, 0, sizeof(TarFileName));

  /* obtain command line args and set up globals */
  ParseArgs(argc, argv);
  DoGlobalSetup();

  /* Obtain dump header  */
  rc = DStream->getDumpHeader(&ThisHead);
  if (!rc){
    fprintf(stderr, "Can't get dump header, giving up\n");
    exit(-1);
  }
  if (DEBUG_HEAVY) PrintDumpHeader(stderr, &ThisHead);

  /* Obtain volume data */
  rc = DStream->getVolDiskData(&ThisVDD);
  if (rc){
    fprintf(stderr, "Can't get VolumeDiskData, giving up\n");
    exit(-1);
  }
  if (DEBUG_HEAVY) PrintVolumeDiskData(stderr, &ThisVDD);
  if (DEBUG_HEAVY) fprintf(stderr, "\n\n");

  if (!RootName)
    RootName = ThisVDD.name;

  /* Initialize the directory package */
  DIR_Init(DIR_DATA_IN_VM);

  /* Get Large Vnodes (i.e. directories) */
  rc = DStream->getVnodeIndex(vLarge, &vLargeCount, &vLargeSize);
  if (rc < 0) {
    fprintf(stderr, "Can't get large vnode index, giving up\n");
    exit(-1);
  }
  if (DEBUG_HEAVY) {
    fprintf(stderr, "vLargeCount = %ld    vLargeSize = %ld\n", 
	  vLargeCount, vLargeSize);
  }

  /* create and initialize LVN list */
  LVNlist = (DumpObject **) malloc(vLargeCount * sizeof(DumpObject *));
  LVNfillcount = 0; 

  /* Now iterate through all the directories */
  if (DEBUG_LIGHT) fprintf(stderr, "Reading and processing directories: ");
  for (i = 0; i < vLargeCount; i++) {
    if (DEBUG_HEAVY) fprintf(stderr, "Starting directory #%i:\n", i);
    rc = ProcessDirectory();
    if (DEBUG_HEAVY) fprintf(stderr, "Ending directory #%i\n\n", i);
    if (rc == -1) break;
    if (DEBUG_LIGHT) fprintf(stderr, "%d ", i);
  }
  if (DEBUG_LIGHT) fprintf(stderr, "\n\n");

  /* At this point we should meet these conditions:
     (a) DumpTable has one entry for every object in dump
     (b) LVNlist has exactly one entry per directory
  */

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\n\n **** Large Vnodes ***\n\n");
    for (i = 0; i < LVNfillcount; i++) {
      char path[CODA_MAXPATHLEN];
      LVNlist[i]->GetFullPath(path, CODA_MAXPATHLEN);
      fprintf(stderr, "%s%s\n", path, LVNlist[i]->children ? "" : " [empty]");
    }
  }

  /* use LVNlist to add empty directories to the tarfile */
  if (!ThisHead.Incremental)
    CreateEmptyDirectories();

  /* Get Small Vnodes (i.e. plain files) */

  DStream->getVnodeIndex(vSmall, &vSmallCount, &vSmallSize);
  if (DEBUG_HEAVY){
    fprintf(stderr, "vSmallCount = %ld    vSmallSize = %ld\n",
	  vSmallCount, vSmallSize);
  }

  if (DEBUG_LIGHT) fprintf(stderr, "Reading and processing small vnodes: ");
  for (i = 0; i < vSmallCount; i++) {
    if (DEBUG_HEAVY) fprintf(stderr, "Starting small vnode #%i:\n", i);
    rc = ProcessFileOrSymlink();
    if (DEBUG_HEAVY) fprintf(stderr, "Ending small vnode #%i:\n", i);
    if (rc == -1) break;
    if (DEBUG_LIGHT) fprintf(stderr, "%d ", i);
  }
  if (DEBUG_LIGHT) fprintf(stderr, "\n\n");

  /* last step is to find hard links and output tar recds for them */
  ProcessHardLinks();

  /* Success */
  delete DStream;
  TarObj.WriteZeroTrailer();
  fclose(TarFile);
  exit(0);
}


void ParseArgs(int argc, char **argv)
{
  for (int i = 1; i < argc; i++) {

    /* [-f <dumpfilename>] */
    if (!strcmp(argv[i], "-f")) {
      if (++i >= argc) goto BogusArgs;
      strncpy(DumpFileName, argv[i], MAXPATHLEN);
      DumpFileName[MAXPATHLEN-1] = 0; /* always, for safety */
      continue;
    }

    /* [-o <tarfilename>] */
    if (!strcmp(argv[i], "-o")) {
      if (++i >= argc) goto BogusArgs;
      strncpy(TarFileName, argv[i], MAXPATHLEN);
      TarFileName[MAXPATHLEN-1] = 0; /* always, for safety */
      continue;
    }

    /* [-d <debuglevel>] */
    if (!strcmp(argv[i], "-d")) {
      if (++i >= argc) goto BogusArgs;
      DebugLevel = atoi(argv[i]);
      continue;
    }

    /* [-hp <hashpower> */
    if (!strcmp(argv[i], "-hp")) {
      if (++i >= argc) goto BogusArgs;
      HashPower = atoi(argv[i]);
      if (HashPower > 30) goto BogusArgs; /* 2^30 buckets is max! */
      continue;
    }

    /* [-rn <rootname>] */
    if (!strcmp(argv[i], "-rn")) {
      if (++i >= argc) goto BogusArgs;
      RootName = argv[i];
      continue;
    }

    goto BogusArgs; /* unknown flag */

  }
  return; /* done with all args */

 BogusArgs:
    printf("Usage: codadump2tar [-f <dumpfilename>] [-o <tarfilename>] [-d <debuglevel>] [-hp <hashpower>] [-rn <rootname>]\n");
    exit(-1);

}

void DoGlobalSetup()
{
  /* separate routine so main() doesn't get too long */

  /* create dump object table */
  DumpTable = new ohashtab((2 << (HashPower-1)), (int (*)(void *)) LowBits);
  if (!DumpTable) {
    fprintf(stderr, "Can't create DumpTable\n");
    exit(-1);
  }

  /* create input  stream */
  DStream = new dumpstream(DumpFileName);
  if (!DStream->isopen()) {
    fprintf(stderr, "Can't create new dumpstream '%s'\n", DumpFileName);
    exit(-1);
  }

  /* create output tar file */
  if (TarFileName[0]) {
    TarFile = fopen(TarFileName, "w");
    if (!TarFile) {
      perror(TarFileName);
      exit(-1);
    }
  }
}

int ProcessDirectory()
{
  /* Called with DStream positioned at a large vnode; reads and 
     processes one large vnode and associated dir pages */

  /* the VnodeDiskObject definition doesn't include space for the ACL */
  char tmpbuf[SIZEOF_LARGEDISKVNODE];
  VnodeDiskObject *vdo = (VnodeDiskObject *)&tmpbuf;

  off_t offset = 0;
  int rc, deleted;
  VnodeId vn;
  DumpObject *CurrentDir;

  /* First get the large vnode */
  memset(vdo, 0, SIZEOF_LARGEDISKVNODE); /* clear to simplify debugging */
  rc = DStream->getNextVnode(vdo, &vn, &deleted, &offset);
  if (rc < 0) return -1;

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\nNext Vnode: deleted = %d    offset = %lld\n",
	    deleted, offset);
    PrintVnodeDiskObject(stderr, vdo, vn);
  } 

  if (deleted) return 0;

  CODA_ASSERT(vdo->type == vDirectory);

  /* Find or create a DumpObject for this large Vnode */
  CurrentDir = GetDumpObj(vn, vdo->uniquifier);

  /* fill in missing pieces */
  CurrentDir->isdir = 1;
  CurrentDir->dir_mtime = (unsigned int) vdo->unixModifyTime;

  /* add pointer to this object in list of large vnodes */
  LVNlist[LVNfillcount++] = CurrentDir;

  /* Then get the associated dir pages */
  PDirInode nextdir;
  if (DStream->readDirectory(&nextdir) < 0) {
    fprintf(stderr, "read of directory pages failed\n");
    exit(-1);
  }

  /* Convert DirInode to DirPage format */
  struct DirHandle dh;
  memset(&dh, 0, sizeof(dh));
  dh.dh_data = DI_DiToDh(nextdir);

  /* Iterate through entries in directory and proces them */
  DH_EnumerateDir(&dh, AddNameEntry, CurrentDir);

  /* Free things allocated in this routine (except vdo) */
  FreeDirectory(nextdir); /* free what readDirectory() allocated */
  free(dh.dh_data); /* also result of conversion */
  return 0;
}

void CreateEmptyDirectories()
{
  /* output tar commands to create skeleton of tree */

  int lvn;
  DumpObject *thisd; /* object being handled now */

  /* Loop through directories, creating tar record for each */

  for (lvn = 0; lvn < LVNfillcount; lvn++) {
    thisd = LVNlist[lvn]; /* next dump object */
    CODA_ASSERT(thisd->isdir);  /* sheer paranoia */
    if (thisd->children) continue;

    if (DEBUG_HEAVY) {
	char path[CODA_MAXPATHLEN];
	thisd->GetFullPath(path, CODA_MAXPATHLEN);
	fprintf(stderr, "[%d]: creating tar output for %s\n", lvn, path);
    }

    /* Fill entries unique to this directory.
       Use 0755 for all directories rather than vdo->modeBits because
       many directories in /coda have bogus mode bits;  causes protection
       failures when trying to create subdirectories via "tar xvf" of output
       tarball in normal Unix systems.
    */
       
    TarObj.Reset();
    TarObj.tr_type = DIRTYPE;
    TarObj.tr_mode = 0755;
    TarObj.tr_uid = 0;
    TarObj.tr_size = 0;
    TarObj.tr_mtime = (unsigned int) thisd->dir_mtime;
    TarObj.GetNameParts(thisd);

    /* fill and output this tar record */
    TarObj.Format();
    TarObj.Output();
  }
}

int ProcessFileOrSymlink()
{
  /* Called with DStream positioned at a small vnode; reads and 
     processes one small vnode and associated data; this is typically
     a file, but might also be a sym link; at end, DStream is positioned
     for next call of ProcessFileOrLink() or is at EOF
  */

  VnodeDiskObject smallv;  /* can be local 'cause no ACL */

  off_t offset = 0;
  int rc, deleted;
  VnodeId vn;
  DumpObject *dobj;

  /* First get the small vnode */
  memset(&smallv, 0, sizeof(smallv)); /* clear to simplify debugging */
  rc = DStream->getNextVnode(&smallv, &vn, &deleted, &offset);
  if (rc < 0) return -1;

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\nNext Vnode: deleted = %d    offset = %lld\n",
	    deleted, offset);
    PrintVnodeDiskObject(stderr, &smallv, vn);
  } 

  if (deleted) return 0;

  /* Find DumpObject for this small Vnode */
  dobj = GetDumpObj(vn, smallv.uniquifier);

  /* Obtain tar record fields for this object */
  TarObj.Reset();
  TarObj.tr_mode = (unsigned int) smallv.modeBits;
  TarObj.tr_uid =  (unsigned int) smallv.owner;
  TarObj.tr_size = (unsigned int) smallv.length; /* set to 0 below if symlink */ 
  TarObj.tr_mtime = (unsigned int) smallv.unixModifyTime;
  TarObj.GetNameParts(dobj);

  if (smallv.type == vSymlink) {
    /* Coda dump format treats symlink value as file content, but tar
       puts symlink value in header record.  So read file content
       to get sym link value before creating and writing out tar recd */

    char *buff = (char *)malloc(smallv.length);
    CODA_ASSERT(buff);
    rc = DStream->CopyBytesToMemory(buff, smallv.length);
    if (rc < 0) {
      fprintf(stderr, "ERROR: Couldn't get sym link from dump file\n");
      exit(-1);
    }
    memcpy(TarObj.tr_linkname, buff, (smallv.length < 100) ? smallv.length : 99);

    /* fill and output this tar record */
    TarObj.tr_type = SYMTYPE;
    TarObj.tr_size = 0; /* override smallv.length; symlinks in tar files have zero length  */
    TarObj.Format();
    TarObj.Output();
    free(buff);
    return 0; 
  }

  /* Else we have a plain file; write out tar recd first */
  TarObj.tr_type = REGTYPE;
  TarObj.Format();
  TarObj.Output();

  /* then copy file content */
  rc = DStream->CopyBytesToFile(TarFile, smallv.length);
  if (rc < 0) {
    fprintf(stderr, "ERROR: Couldn't read file content from dump file\n");
    exit(-1);
  }

  /* We are done!  DStream should be at next small vnode (or EOF) */
  return 0;
}


/* Called only after tar records have been created for all dumpobjects;
   hence targets of all hard links created when untaring are guaranteed to exist */
void ProcessHardLinks()
{
  ohashtab_iterator dti(*DumpTable);  /* iterator over all buckets */
  DumpObject *dobj;

  /* Examine every object in the DumpTable and see if it has more than 
     one; if yes, create a hard link for each additional name */

  while ((dobj = (DumpObject *)dti())) {
    if (dobj->links <= 1) continue; /* common case */

    for (int i = 1; i < dobj->links; i++)
      CreateHardLinkRecd(dobj, i);
  }
}

/* Creates a tar recd that will make new a hard link to old */
void CreateHardLinkRecd(DumpObject *dobj, int idx)
{

  if (DEBUG_HEAVY) {
    char oldpath[CODA_MAXPATHLEN], newpath[CODA_MAXPATHLEN];
    dobj->GetFullPath(oldpath, CODA_MAXPATHLEN);
    dobj->GetFullPath(newpath, CODA_MAXPATHLEN, idx);
    fprintf(stderr, "Hard link: new = '%s'\n", newpath);
    fprintf(stderr, "           old = '%s'\n", oldpath);
  }

    TarObj.Reset();
    TarObj.tr_type = LNKTYPE;
    TarObj.GetNameParts(dobj, idx);
    dobj->GetFullPath(TarObj.tr_linkname, 99);
    TarObj.Format();
    TarObj.Output();
}



/* ----- DumpObject methods ----- */

DumpObject::DumpObject(VnodeId v, Unique_t uq)
{
  /* constructor*/
  oid.vnode = v;
  oid.uniquifier = uq;

  links = 0;
  parents = NULL;
  components = NULL;

  isdir = 0;
  children = 0;
}

void DumpObject::AddParent(DumpObject *parent, const char *component)
{
  /* Give dump object a pathname; multiple names are due to hard links */

  DumpObject **newparents;
  char **newcomponents;
  int newlen = links + 1;

  if (!links) {
      newparents = (DumpObject **)malloc(newlen*sizeof(DumpObject *));
      newcomponents = (char **)malloc(newlen*sizeof(char *));
  }
  else
  {
      if (DEBUG_HEAVY) {
	  fprintf(stderr, "HARDLINK: old linkcount == %d\n", links);
	  fprintf(stderr, "          original name: '%s'\n", components[0]);
	  fprintf(stderr, "               new name: '%s'\n", component);
      }
      newparents = (DumpObject **)realloc(parents, newlen*sizeof(DumpObject *));
      newcomponents = (char **)realloc(components, newlen*sizeof(char *));
  }
  CODA_ASSERT(newparents && newcomponents);

  parents = newparents;
  components = newcomponents;

  parents[links] = parent;
  components[links] = strdup(component);
  CODA_ASSERT(components[links]); 

  links++;
  if (parent)
      parent->children++;
}

/* Fill buf with full pathname of the parent (or return 0) */
size_t DumpObject::GetPrefix(char *buf, size_t len, int idx)
{
    if (idx >= links)
	return 0;
    
    return parents[idx]->GetFullPath(buf, len);
}

/* Fill buf with the basename of this object */
size_t DumpObject::GetComponent(char *buf, size_t len, int idx)
{
    size_t ret;
    int n;

    if (!len) return 0;

    if (idx >= links)
    {
	if (oid.vnode == 1 && oid.uniquifier == 1)
	     n = snprintf(buf, len, "%s", RootName);
	else n = snprintf(buf, len, "%s/lost+found/%08x.%08x",
			  RootName, oid.vnode, oid.uniquifier);
	if (n < 0) {
	    *buf = '\0';
	    ret = 0;
	} else
	    ret = (size_t)n;
    }
    else
    {
	strncpy(buf, components[idx], len);
	ret = strlen(buf);
    }

    /* Now what do we do with truncated names... */
    if (ret >= len) {
	switch(len) {
	default: buf[len-4] = '~';
	case 3:  buf[len-3] = '~';
	case 2:  buf[len-2] = '~';
	case 1:  buf[len-1] = '\0';
	case 0:  break;
	}
	ret = len-1;
    }
    return ret;
}

/* Fill buf with full pathname of this object */
size_t DumpObject::GetFullPath(char *buf, size_t len, int idx)
{
    size_t n;
    if (!len) return 0;

    n = GetPrefix(buf, len, idx);
    if (n) {
	if (n < len-2) {
	    buf[n++] = '/';
	    buf[n] = '\0';
	}
	buf += n;
	len -= n;
    }
    n += GetComponent(buf, len, idx);
    return n;
}


/* ----- TarRecd methods ----- */

TarRecd::TarRecd()
{
  Reset();
}

void TarRecd::Reset()
{
  struct posix_header *tblk = &tarblock.header; 

  /* Initialize header block to clean state 
     Fields are Ascii, with numbers expressed in octal.
     No trailing null except for name fields explicitly stated below.
     Zero-fill initialization guarantees trailing NULL if needed.
  */

  memset(tarblock.buffer, 0, BLOCKSIZE); /* zerofill */

  /* set fields that never change for us */
  memset(tblk->gid, '0', 8);
  memcpy(tblk->magic, TMAGIC, TMAGLEN-1); /* trailing NULL */
  memcpy(tblk->version, TVERSION, TVERSLEN);

  /* also clear all the public fields */
  tr_type = 0;
  tr_mode = tr_uid = tr_size = tr_mtime = 0;
  memset(tr_name, 0, 100);
  memset(tr_linkname, 0, 100);
  memset(tr_prefix, 0, 155);
}


void TarRecd::GetNameParts(DumpObject *obj, int idx)
{
    obj->GetPrefix(TarObj.tr_prefix, 155, idx);
    obj->GetComponent(TarObj.tr_name, 100, idx);
}


void TarRecd::Format()
{
  struct posix_header *tblk = &tarblock.header; 
  char temp[100];  /* temporary to avoid NULLs in TarRecd from sprintf() */
  unsigned int sum; /* for checksum computation */
  int i;

  memcpy(&tblk->typeflag, &tr_type, 1);
  sprintf(temp,     "%08o",  tr_mode);
  memcpy(tblk->mode, temp, 8);
  sprintf(temp,      "%08o",  tr_uid);
  memcpy(tblk->uid, temp, 8);
  sprintf(temp,     "%012o", tr_size);  
  memcpy(tblk->size, temp, 12);
  sprintf(temp,    "%012o", tr_mtime);
  memcpy(tblk->mtime, temp, 12);

  /* copy name info; leave NULL in last byte from Reset() */
  memcpy(tblk->name, tr_name, 99); 
  memcpy(tblk->linkname, tr_linkname, 99); 
  memcpy(tblk->prefix, tr_prefix, 154);

  /* Compute checksum */
  memset(tblk->chksum, ' ', 8);       /* 8 blanks */

  sum = 0; 
  for (i = 0; i < BLOCKSIZE; i++) {
    sum += (unsigned int) tarblock.buffer[i];
  }
  sprintf(temp, "%08o", sum);
  memcpy(tblk->chksum, temp, 8);
}

void TarRecd::Output()
{
  int rc;

  rc = fwrite(&tarblock, BLOCKSIZE, 1, TarFile);
  if(rc != 1) {
    perror("tar file output error");
    exit(-1);
    }
}

void TarRecd::WriteZeroTrailer()
{
  /* end tar file with two zero blocks */
  memset(tarblock.buffer, 0, BLOCKSIZE); /* zerofill */
  Output();
  Output();
}


/* ------ comparator function definitions ------ */

/* for DumpTable->FindObject() */
int CompareDumpOid(DumpObject *dobj, objectid_t *testid)
{
    if (dobj->oid.vnode != testid->vnode) return(0);
    if (dobj->oid.uniquifier != testid->uniquifier) return(0);
    return (1);
}

/* ------ helper function definitions ------ */

DumpObject *GetDumpObj(VnodeId vv, Unique_t uu)
{
  /* Checks if DumpObject already exists for vnode-unqiuifier pair; 
     returns pointer  to that DumpObject or NULL */

  DumpObject dummy (0,0); /* just to alloc space */
  DumpObject *dobj;

  dummy.oid.vnode = vv;
  dummy.oid.uniquifier = uu;
  dobj = (DumpObject *)DumpTable->FindObject(&dummy, &dummy.oid, 
					     (otagcompare_t)CompareDumpOid);
  if (!dobj) {
      dobj = new DumpObject(vv, uu);
      CODA_ASSERT(dobj);
      DumpTable->insert(dobj, dobj);
  }
  return dobj;
}


void FreeDirectory(PDirInode pdiri)
{
  /* Free items malloc'ed by dumpstream::readDirectory() */

  /* This relies on di_pages[] elements being non-zero 
     only if allocated; would have been much better for DirInode
     to have an explicit count */

  for (int i = 0; i < DIR_MAXPAGES; i++){
    if (pdiri->di_pages[i]) free (pdiri->di_pages[i]);
    //else break;
  }
  free(pdiri);  /* get rid of the DirInode itself */
}


int LowBits(DumpObject *obj)
{
  /* hash function: use low order bits of object's vnode number */
  int bucket, shift;

  shift = 32-HashPower;
  bucket = (obj->oid.vnode) << shift;
  bucket = bucket >> shift;
  return(bucket);  
}

int AddNameEntry(struct DirEntry *de, void *hook)
{
  /* AddNameEntry() is a "hookproc" for use by DIR_EnumerateDir() */

  DumpObject *newobj;
  DumpObject *parent = (DumpObject *)hook;

  /* coda dir pkg uses net order; convert just once */
  VnodeId thisv = ntohl(de->fid.dnf_vnode); 
  Unique_t thisu = ntohl(de->fid.dnf_unique);
  if (DEBUG_HEAVY) {
    fprintf(stderr, "    0x%08lx.%08lx  %s", (unsigned long) thisv,
	    (unsigned long) thisu, de->name);
  }

  /* "." and ".." entries play no useful role for us; just ignore */
  if (de->name[0] == '.' && (de->name[1] == '\0' ||
       (de->name[1] == '.' && de->name[2] == '\0'))) {
      if (DEBUG_HEAVY) fprintf(stderr, "\t[skipping]\n");
      return(0);
  }

  /* Find or create dump object entry; if hard links across directories
     are allowed, this can be unpredictable.
  */
  newobj = GetDumpObj(thisv, thisu);

  if (DEBUG_HEAVY)
      fprintf(stderr, "\t[%s dump object]\n", newobj->links? "found":"created");

  /* newobj guaranteed to be defined at this point */
  parent->children++;
  newobj->AddParent(parent, de->name);

  return(0); /* continue enumeration */
}

/* NOTES: (mostly bugs) Satya, May 04 

1. ACL in dump has group and user ids; totally unportable;
   dump should be written out in external form (ascii only);
   need a version change plus

*/

