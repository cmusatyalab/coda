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
   The iterative process of discovering pathnames supports this.

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

/* pathname of object */
typedef struct{
  char *fullpathname; /* pointer to malloc'ed string */
  int prefixlen; /* how many chars in prefix (sans trailing slash) */
} objname_t;

/* entry in hash table for one object in dump */
class DumpObject:public olink {
public:
  objectid_t oid;
  int linkcount; /* 0 initially; typically 1 at end; > 1 if hard links */
  objname_t **onarray; /* array of linkcount malloc'ed names; NULL initially */
  int slashcount; /* how many slashes in onarray[0]->fullpathname (depth in tree) */

  /* the following are only used by directories */
  unsigned int isdir;
  unsigned int dir_owner;
  unsigned int dir_size;
  unsigned int dir_mtime;

  DumpObject(VnodeId, Unique_t);
  void AddPathname(const char *prefix, const char *lastcomponent);
  const char *GetPathname(); /* NULL or first pathname */
};

/* one entry in Namelist */
class NameListEntry: public olink{
public:
  DumpObject *parentdir;  /* directory in which name occurs */
  DumpObject *mydumpobj;   /* object corr to this nle */
  char *component_name; /* malloc'ed string */

  NameListEntry(DumpObject *, DumpObject *, char *);
  const char *NameOfParent(); /* non-NULL only if parendir's name is known */
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
  void GetNameParts(objname_t *);  /* extracts name fields */
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
FILE *TarFile;  /* open file handle for output */

/* from "-rn xxx" on command line; if not specified, uses 
   volume name from dump, prefixed by "./" */
char *RootName;

int DebugLevel = 0; /* set by "-d xxx" command line switch */
#define DEBUG_HEAVY  ((DebugLevel >= 10))
#define DEBUG_LIGHT  ((DebugLevel > 0) && (DebugLevel < 10))
#define DEBUG_ON     (DEBUG_LIGHT || DEBUG_HEAVY)

/* Hash table of all objects found in dump */
ohashtab *DumpTable;

/* a singly linked unordered list of all names found in directories */
olist NameList; 

/* stuff read from dump */
struct DumpHeader ThisHead;    /* info about this dump */
VolumeDiskData ThisVDD;       /* info pertaining to entire volume */

/* Array for sorting large vnodes by directory depth */
DumpObject **LVNsortarray; /* malloc'ed when no. of large vnodes known */
int  LVNfillcount; /* no of entries filled in LVNsortarray */


/* CurrentDir is set on each iteration through large Vnode objects; 
   this is the parentdir value for all the NameList entries created 
   for names in this directory */
DumpObject *CurrentDir; 

/* TarObj is used to create and write out tar records */
TarRecd TarObj;


#define DEFAULT_HASHPOWER 12 /* 2^HashPower is # of buckets in hash table */
int HashPower = DEFAULT_HASHPOWER; /* can be set by "-hp x" on command line */


/* ----- Forward refs to non-method procs defined later in this file ----- */

/* steps of main() */
void ParseArgs(int, char **);
void DoGlobalSetup();
void ProcessDirectory();
void SanityCheckLists();
void DiscoverPathnames();
void VerifyEverythingNamed();
void CreateSkeletonTree();
void ProcessFileOrSymlink();
void ProcessHardLinks();

/* comparator functions for generic routines like qsort() and FindObject() */
int CompareDumpOid(DumpObject *, objectid_t *);
int CompareNLoid(NameListEntry *, objectid_t *);
int CompareDepths(const void *, const void *);

/* helper routines */
DumpObject *DoesDumpObjectExist(VnodeId, Unique_t);
int LowBits(DumpObject *);
void FreeDirectory(PDirInode);
int AddNameEntry(struct DirEntry *, void *);
void NameTheRoot();
void CreateHardLinkRecd(objname_t *, objname_t *);


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
  if (ThisHead.Incremental) {
    fprintf(stderr, "Can't restore incremental dump!\n");
    exit(-1);
  }


  /* Obtain volume data */
  rc = DStream->getVolDiskData(&ThisVDD);
  if (rc){
    fprintf(stderr, "Can't get VolumeDiskData, giving up\n");
    exit(-1);
  }
  if (DEBUG_HEAVY) PrintVolumeDiskData(stderr, &ThisVDD);
  if (DEBUG_HEAVY) fprintf(stderr, "\n\n");

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

  /* create and initialize LVN sort array */
  LVNsortarray = (DumpObject **) malloc(vLargeCount * sizeof(DumpObject *));
  LVNfillcount = 0; 

  /* Now iterate through all the directories */
  if (DEBUG_LIGHT) fprintf(stderr, "Reading and processing directories: ");
  for (i = 0; i < vLargeCount; i++) {
    if (DEBUG_HEAVY) fprintf(stderr, "Starting directory #%i:\n", i);
    ProcessDirectory();
    if (DEBUG_HEAVY) fprintf(stderr, "Ending directory #%i\n\n", i);
    if (DEBUG_LIGHT) fprintf(stderr, "%d ", i);
  }
  if (DEBUG_LIGHT) fprintf(stderr, "\n\n");

  /* At this point we should meet these conditions:
     (a) DumpTable has one entry for every object in dump
     (b) NameList has one entry for each directory entry seen (sans "..")
     (c) Somewhere in DumpTable is the root directory of the volume
     (d) LVNsortarray has exactly one entry per directory (not sorted yet)
  */

  /* Verify that invariants are met */
  SanityCheckLists(); 

  /* Iterate over NameList and find pathnames of all dump objects */
  DiscoverPathnames();

  /* We should now know the name of every dump object; make sure of this */
  VerifyEverythingNamed();

  /* sort directory names by depth in tree */
  qsort(LVNsortarray, LVNfillcount, sizeof(DumpObject *), CompareDepths);

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\n\n **** Sorted by depth ***\n\n");
    for (i = 0; i < LVNfillcount; i++) {
      objname_t **onx = LVNsortarray[i]->onarray;
      fprintf(stderr, "%s [%d]\n", onx[0]->fullpathname, onx[0]->prefixlen);
    }
  }

  /* use LVNsortarray to create skeleton tree in tarfile */
  CreateSkeletonTree();

  /* Get Small Vnodes (i.e. plain files) */

  DStream->getVnodeIndex(vSmall, &vSmallCount, &vSmallSize);
  if (DEBUG_HEAVY){
    fprintf(stderr, "vSmallCount = %ld    vSmallSize = %ld\n",
	  vSmallCount, vSmallSize);
  }

  if (DEBUG_LIGHT) fprintf(stderr, "Reading and processing small vnodes: ");
  for (i = 0; i < vSmallCount; i++) {
    if (DEBUG_HEAVY) fprintf(stderr, "Starting small vnode #%i:\n", i);
    ProcessFileOrSymlink();
    if (DEBUG_HEAVY) fprintf(stderr, "Ending small vnode #%i:\n", i);
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


void ParseArgs(int argc, char **argv){
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

void DoGlobalSetup() {
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
  if (TarFileName[0] == 0) {
    TarFile = stdout;
  }
  else {
    TarFile = fopen(TarFileName, "w");
    if (!TarFile) {
      perror(TarFileName);
      exit(-1);
    }
  }

}

void ProcessDirectory()
{
  /* Called with DStream positioned at a large vnode; reads and 
     processes one large vnode and associated dir pages */

  /* the VnodeDiskObject definition doesn't include space for the ACL */
  char tmpbuf[SIZEOF_LARGEDISKVNODE];
  VnodeDiskObject *vdo = (VnodeDiskObject *)&tmpbuf;

  off_t offset = 0;
  int rc, deleted;
  VnodeId vn;

  /* First get the large vnode */
  memset(vdo, 0, SIZEOF_LARGEDISKVNODE); /* clear to simplify debugging */
  rc = DStream->getNextVnode(vdo, &vn, &deleted, &offset);
  if (rc < 0) {
    fprintf(stderr, "getNextVnode() failed for vnode 0x%08lx\n", vn);
    exit(-1);
  }

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\nNext Vnode: deleted = %d    offset = %ld\n",
	    deleted, offset);
    PrintVnodeDiskObject(stderr, vdo, vn);
  } 

  CODA_ASSERT(vdo->type == vDirectory);

  /* Find or create a DumpObject for this large Vnode */
  CurrentDir = DoesDumpObjectExist(vn, vdo->uniquifier);
  if (!CurrentDir) {
    /* Large vnode encountered before first mention of its name in a directory */
    CODA_ASSERT(CurrentDir = new DumpObject(vn, vdo->uniquifier));
    DumpTable->insert(CurrentDir, CurrentDir);
  }

  CurrentDir->isdir = 1; /* fill in missing pieces */
  CurrentDir->dir_owner = (unsigned int) vdo->owner;
  CurrentDir->dir_size  = (unsigned int) vdo->length;  
  CurrentDir->dir_mtime = (unsigned int) vdo->unixModifyTime;

  /* add pointer to this object in sort array */
  LVNsortarray[LVNfillcount++] = CurrentDir;

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
  DH_EnumerateDir(&dh, AddNameEntry, 0);

  /* Free things allocated in this routine (except vdo) */
  FreeDirectory(nextdir); /* free what readDirectory() allocated */
  free(dh.dh_data); /* also result of conversion */
}

void SanityCheckLists() {
  /* Routine to verify invariants of dump objects and name list entries */

  ohashtab_iterator dti(*DumpTable);  /* iterator over all buckets */
  DumpObject *dobj;
  int dobjcount;
  NameListEntry *nlmatch;

  /* Every object in the DumpTable should have been named in
     at least in one directory (may be more with hard links) */

  dobjcount = 0;
  while ((dobj = (DumpObject *)dti())) {
    nlmatch = (NameListEntry *) NameList.FindObject(&dobj->oid, 
						    (otagcompare_t) CompareNLoid);

    if (!nlmatch) {
      fprintf(stderr, "SanitCheckLists(): Couldn't find nle for 0x%08lx.0x%08lx\n",
	      dobj->oid.vnode, dobj->oid.uniquifier);
      exit(-1);
    }

    dobjcount++;
  }

  if (DEBUG_HEAVY) fprintf(stderr, "Found %d dump objects, all named\n", dobjcount);
}


void DiscoverPathnames() {
  /* This routine starts with a full DumpTable that has no pathname info,
     and a full NameList, all of whose entries have parents with
     unknown pathnames. The routine iterates a shrinking NameList, 
     discovering a new pathname each time a NameList entry's parent has
     a known pathname.  On exit, NameList is empty and every object 
     in DumpTable should have a pathname (may have more than one 
     in case of  hard links)
  */

  int itercount = 0; /* debugging */
  olist_iterator nliter(NameList);

  /* First give the root object its name */
  NameTheRoot();

  /* Then keep iterating over NameList until it vanishes */
  while ((NameList.count() > 0)) {
    NameListEntry *nextnle;

    if (DEBUG_HEAVY) 
      fprintf(stderr, "DiscoverPathnames: iteration #%d\n", itercount);
    nliter.reset(); /* clean start on each iteration */

    /* walk the list */
    while ((nextnle = (NameListEntry *)nliter())) {
      const char *prefix = nextnle->NameOfParent();

      if (!prefix) continue;
      /* We now know name of this NameListEntry! */

      nextnle->mydumpobj->AddPathname(prefix, nextnle->component_name);
      CODA_ASSERT(NameList.remove(nextnle));  /* shrink NameList */
    }
    itercount++;
  }
}



void VerifyEverythingNamed() {
  /* Sanity check routine, to make sure we aren't missing any names */

  ohashtab_iterator dti(*DumpTable);  /* iterator over all buckets */
  DumpObject *dobj;

  while ((dobj = (DumpObject *)dti())) {
    if (!dobj->linkcount) {
      fprintf(stderr, "VerifyEverythingNamed():  0x%08lx.%08lx is not named\n",
	      (unsigned long) dobj->oid.vnode, 
	      (unsigned long) dobj->oid.uniquifier);
      exit(-1);
    }
  }

  if (DEBUG_HEAVY) fprintf(stderr, "VerifyEveythingNamed(): success!\n");
}

void CreateSkeletonTree() {
  /* output tar commands to create skeleton of tree */

  int lvn;
  DumpObject *thisd; /* object being handled now */
  objname_t *thisn; /* pointer to this object's name */

  /* Loop through directories, creating tar record for each */

  for (lvn = 0; lvn < LVNfillcount; lvn++) {
    thisd = LVNsortarray[lvn]; /* next dump object */
    thisn = thisd->onarray[0]; /* name of dump object */
    CODA_ASSERT(thisd->isdir);  /* sheer paranoia */

    if (DEBUG_HEAVY) fprintf(stderr, "[%d]: creating tar output for %s\n",
			    lvn, thisn->fullpathname);

    /* Fill entries unique to this directory.
       Use 0755 for all directories rather than vdo->modeBits because
       many directories in /coda have bogus mode bits;  causes protection
       failures when trying to create subdirectories via "tar xvf" of output
       tarball in normal Unix systems.
    */
       
    TarObj.Reset();
    TarObj.tr_type = DIRTYPE;
    TarObj.tr_mode = 0755;
    TarObj.tr_uid = (unsigned int) thisd->dir_owner;
    TarObj.tr_size = (unsigned int) thisd->dir_size;
    TarObj.tr_mtime = (unsigned int) thisd->dir_mtime;
    TarObj.GetNameParts(thisn);

    /* fill and output this tar record */
    TarObj.Format();
    TarObj.Output();
  }
}

void ProcessFileOrSymlink() {
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
  if (rc < 0) {
    fprintf(stderr, "getNextVnode() failed for vnode 0x%08lx\n", vn);
    exit(-1);
  }

  if (DEBUG_HEAVY) {
    fprintf(stderr, "\nNext Vnode: deleted = %d    offset = %ld\n",
	    deleted, offset);
    PrintVnodeDiskObject(stderr, &smallv, vn);
  } 

  /* Find DumpObject for this small Vnode */
  dobj = DoesDumpObjectExist(vn, smallv.uniquifier);
  CODA_ASSERT(dobj);

  /* Obtain tar record fields for this object */
  TarObj.Reset();
  TarObj.tr_mode = (unsigned int) smallv.modeBits;
  TarObj.tr_uid =  (unsigned int) smallv.owner;
  TarObj.tr_size = (unsigned int) smallv.length; /* set to 0 below if symlink */ 
  TarObj.tr_mtime = (unsigned int) smallv.unixModifyTime;
  TarObj.GetNameParts(dobj->onarray[0]);

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
    return; 
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
}


/* Called only after tar records have been created for all dumpobjects;
   hence targets of all hard links created when untaring are guaranteed to exist */
void ProcessHardLinks() {
  ohashtab_iterator dti(*DumpTable);  /* iterator over all buckets */
  DumpObject *dobj;

  /* Examine every object in the DumpTable and see if it has more than 
     one; if yes, create a hard link for each additional name */

  while ((dobj = (DumpObject *)dti())) {
    if (dobj->linkcount == 1) continue; /* common case */

    for (int i = 1; i < dobj->linkcount; i++) {
      CreateHardLinkRecd (dobj->onarray[0], dobj->onarray[i]);
    }
  }
}

/* Creates a tar recd that will make new a hard link to old */
void CreateHardLinkRecd(objname_t *oldname, objname_t *newname) {

  if (DEBUG_HEAVY) {
    fprintf(stderr, "Hard link: new = '%s'\n", newname->fullpathname);
    fprintf(stderr, "           old = '%s'\n", oldname->fullpathname);
  }

    TarObj.Reset();
    TarObj.tr_type = LNKTYPE;
    TarObj.GetNameParts(newname);
    strncpy(TarObj.tr_linkname, oldname->fullpathname, 99);
    TarObj.Format();
    TarObj.Output();
}



/* ----- DumpObject methods ----- */

DumpObject::DumpObject(VnodeId v, Unique_t uq)
{
  /* constructor*/
  oid.vnode = v;
  oid.uniquifier = uq;
  linkcount = 0;
  onarray = NULL; 
  slashcount = 0;
  isdir = 0;
}

void DumpObject::AddPathname(const char *prefix, const char *lastcomponent) {
  /* Give dump object a pathname; multiple names are due to hard links */

  char *newpathname;
  int size;

  /* +2 --> one for slash, one for trailing null */
  size = (prefix ? strlen(prefix) : 0) + strlen(lastcomponent) + 2;

  /* construct the pathname */
  if (prefix) {
    newpathname = (char *) malloc(size);
    CODA_ASSERT(newpathname);
    strcpy(newpathname, prefix);
    strcat(newpathname, "/");
    strcat(newpathname, lastcomponent);
  }
  else newpathname = strdup(lastcomponent);

  if (DEBUG_HEAVY) fprintf(stderr, "Adding pathname: '%s'\n", newpathname);

  /* create empty entry in array of names for this dump object */ 
  if (linkcount) { /* at least one name already exists */
    objname_t **newarray;

    if (DEBUG_HEAVY) {
      fprintf(stderr, "HARDLINK: old linkcount == %d\n", linkcount);
      fprintf(stderr, "          original name: '%s'\n", onarray[0]->fullpathname);
      fprintf(stderr, "               new name: '%s'\n", newpathname);
    }

    newarray = (objname_t **)realloc(onarray, ((linkcount+1)*sizeof(objname_t *)));
    CODA_ASSERT(newarray);
    newarray[linkcount] = (objname_t *) malloc(sizeof(objname_t));
    CODA_ASSERT(newarray[linkcount]);
    onarray = newarray;
  }
  else { /* first pathname for this object */
    /* note tree depth */
    slashcount = 0;
    for (int i = 0; i < size;  i++){
      if (newpathname[i] == '/') slashcount++;
    }
    /* create 1-entry array */
    onarray = (objname_t **) malloc(sizeof(objname_t *));
    CODA_ASSERT(onarray);
    onarray[0] = (objname_t *) malloc(sizeof(objname_t));
    CODA_ASSERT(onarray[0]);
  }

  /* fill newly-created entry */
  onarray[linkcount]->fullpathname = newpathname;
  onarray[linkcount]->prefixlen = prefix ? strlen(prefix) : 0;

  /* note that we now have one more name */
  linkcount++; 
}

/* Return (first) pathname of object or NULL */
const char *DumpObject::GetPathname()
{
    return (linkcount ? onarray[0]->fullpathname : NULL);
}



/* ----- NameList methods ----- */

NameListEntry::NameListEntry(DumpObject *parent, DumpObject *me, char *nm)
{
    parentdir = parent;
    mydumpobj = me;

    int size = strlen(nm) + 1;
    CODA_ASSERT(component_name = (char *) malloc(size));
    strcpy(component_name, nm);
}

const char *NameListEntry::NameOfParent()
{
    return parentdir->GetPathname();
}

/* ----- TarRecd methods ----- */

TarRecd::TarRecd() {
  Reset();
}

void TarRecd::Reset() {
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


void TarRecd::GetNameParts(objname_t *obn)
{
    if (obn->prefixlen) {/* skip trailing slash */
      strncpy(TarObj.tr_name, &obn->fullpathname[(obn->prefixlen)+1], 100);
      int lx = (obn->prefixlen < 155) ? obn->prefixlen : 155;
      memcpy(TarObj.tr_prefix, obn->fullpathname, lx);
    }
    else { /* no prefix */
      strncpy(TarObj.tr_name, obn->fullpathname, 100);
    }
}


void TarRecd::Format() {
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

void TarRecd::Output() {
  int rc;

  rc = fwrite(&tarblock, BLOCKSIZE, 1, TarFile);
  if(rc != 1) {
    perror("tar file output error");
    exit(-1);
    }
}

void TarRecd::WriteZeroTrailer() {
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

/* For NameList->FindObject() */
int CompareNLoid(NameListEntry *nle, objectid_t *testid)
{
    return CompareDumpOid(nle->mydumpobj, testid);
}

/* for qsort() of LVNsortarray */
int CompareDepths(const void *x, const void *y)
{
  DumpObject *a = *((DumpObject **)x);
  DumpObject *b = *((DumpObject **)y);
  objname_t *an, *bn;

  if (a->slashcount <  b->slashcount) return(-1);
  if (a->slashcount >  b->slashcount) return(1);

  /* (a->slashcount == b->slashcount) */
  /* use lexical ordering on names of equal depth */
  an = (a->onarray)[0];
  bn = (b->onarray)[0];
  return(strcmp(an->fullpathname, bn->fullpathname));
}


/* ------ helper function definitions ------ */

DumpObject *DoesDumpObjectExist(VnodeId vv, Unique_t uu)
{
  /* Checks if DumpObject already exists for vnode-unqiuifier pair; 
     returns pointer  to that DumpObject or NULL */

  DumpObject dummy (0,0); /* just to alloc space */
  DumpObject *dobj;

  dummy.oid.vnode = vv;
  dummy.oid.uniquifier = uu;
  dobj = (DumpObject *)DumpTable->FindObject(&dummy, &dummy.oid, 
					     (otagcompare_t)CompareDumpOid);
  return(dobj);
}


void FreeDirectory(PDirInode pdiri)
{
  /* Free items malloc'ed by dumpstream::readDirectory() */

  /* This relies on di_pages[] elements being non-zero 
     only if allocated; would have been much better for DirInode
     to have an explicit count */

  for (int i = 0; i < DIR_MAXPAGES; i++){
    if (pdiri->di_pages[i]) free (pdiri->di_pages[i]);
    else break;
  }
  free(pdiri);  /* get rid of the DirInode itself */
}


int LowBits(DumpObject *obj){
  /* hash function: use low order bits of object's vnode number */
  int bucket, shift;

  shift = 32-HashPower;
  bucket = (obj->oid.vnode) << shift;
  bucket = bucket >> shift;
  return(bucket);  
}

int AddNameEntry(struct DirEntry *de, void *hook){
  /* AddNameEntry() is a "hookproc" for use by DIR_EnumerateDir() */

  NameListEntry *nle;
  DumpObject *newobj;

  /* coda dir pkg uses net order; convert just once */
  VnodeId thisv = ntohl(de->fid.dnf_vnode); 
  Unique_t thisu = ntohl(de->fid.dnf_unique);
  if (DEBUG_HEAVY) {
    fprintf(stderr, "    0x%08lx.%08lx  %s", (unsigned long) thisv,
	    (unsigned long) thisu, de->name);
  }

  /* "." and ".." entries play no useful role for us; just ignore */
  if (de->name[0] == '.' && (de->name[1] == '\0' ||
       (de->name[1] == '.' && de->name[2] == '\0')))
      return(0);

  /* Find or create dump object entry; if hard links across directories
     are allowed, this can be unpredictable.
  */
  newobj = DoesDumpObjectExist(thisv, thisu);
  if (!newobj) {
    CODA_ASSERT(newobj = new DumpObject(thisv, thisu));
    DumpTable->insert(newobj, newobj);
    if (DEBUG_HEAVY) fprintf(stderr, "    [created dump object]\n");
  } else
    if (DEBUG_HEAVY) fprintf(stderr, "    [found dump object]\n");

  /* newobj guaranteed to be defined at this point */

   /* Create a name list entry */
  nle = new NameListEntry(CurrentDir, newobj, de->name);
  CODA_ASSERT(nle);
  NameList.insert(nle);

  return(0); /* continue enumeration */
}

/* Routine to bootstrap the name discovery process */
void NameTheRoot()
{
  DumpObject *rootobj;

  /* Somewhere in the DumpTable is the root directory; find it */
  rootobj = DoesDumpObjectExist(1, 1);
  if (!rootobj) {
    fprintf(stderr, "Can't find root object in DumpTable\n");
    exit(-1);
  }

  if (DEBUG_HEAVY) {
    fprintf(stderr, "Found root object: 0x%08lx.%08lx\n",
	    (unsigned long) rootobj->oid.vnode, 
	    (unsigned long) rootobj->oid.uniquifier);
  }

  /* If needed, construct pathname of the root object */
  if (!RootName) {
    int bufsize = strlen(ThisVDD.name) + 1;
    RootName = (char *) malloc (bufsize);
    CODA_ASSERT(RootName);
    strcpy(RootName, ThisVDD.name);
    for (int i = 0; i < bufsize; i++) {
      /* change dubious chars in name to avoid shell trouble later */
      if (RootName[i] == ':') RootName[i] = '-';
      /* add other common substitutions here based on experience */
    }
  }

  /* Assign the root its name */
  rootobj->AddPathname(NULL, RootName);
  if (DEBUG_HEAVY) fprintf(stderr, "Root pathname = '%s'\n", RootName);
}

/* NOTES: (mostly bugs) Satya, May 04 

1. ACL in dump has group and user ids; totally unportable;
   dump should be written out in external form (ascii only);
   need a version change plus

*/

