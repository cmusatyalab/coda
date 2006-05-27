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
                           none currently

#*/

/*
 * Module to define a dump file stream class, with dedicated functions
 * to get and put information into it.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>

#ifdef __cplusplus
}
#endif

#include <voltypes.h>
#include <vcrcommon.h>
#include <cvnode.h>
#include <volume.h>
#include <codadir.h>
#include <dump.h>
#include "dumpstream.h"
#include <util.h>
#include <coda_largefile.h>

/*************************** From readstuff.c */

int GetShort(FILE *stream, unsigned short *sp) { /* Assuming two byte words. */
    unsigned char a, b;
    a = fgetc(stream);
    b = fgetc(stream);
    if (feof(stream))
	return FALSE;

    unsigned short v = (a << 8) | b;
    *sp = v;
    return TRUE;
}

int GetInt32(FILE *stream, unsigned int *lp)
{
    unsigned char a, b, c, d;
    a = fgetc(stream);
    b = fgetc(stream);
    c = fgetc(stream);
    d = fgetc(stream);
    if (feof(stream))
	return FALSE;

    unsigned int v = (a << 24) | (b << 16) | (c << 8) | d;
    *lp = v;
    return TRUE;
}

int GetString(FILE *stream, char *to, unsigned int max)
{
    unsigned int len, tail = 0;
    if (!GetInt32(stream, &len))
	return FALSE;

    if (len > max) {	/* Ensure we only use max room */
	LogMsg(0, VolDebugLevel, stdout, "GetString: String longer than max (%d>%d) truncating.",len,max);
	tail = len - max + 1;
	len = max - 1;
    } else if (len == max) {
	/* the dumper 'should' have null-terminated the string, but we add
	 * the '\0' ourselves just in case */
	tail = 1;
	len = max - 1;
    }
    
    while (len--)
	*to++ = fgetc(stream);

    *to = '\0';		/* Make it null terminated */

    /* remove any trailing characters */
    while (tail--)
	(void)fgetc(stream);

    if (feof(stream))
	return FALSE;

    return TRUE;
}

int GetByteString(FILE *stream, register byte *to, register int size)
{
    while (size--)
	*to++ = fgetc(stream);

    if (feof(stream))
	return FALSE;

    return TRUE;
}

int GetVV(FILE *stream, register vv_t *vv)
{
    int tag;
    while ((tag = fgetc(stream)) > D_MAX && tag) {
	switch (tag) {
	    case '0':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site0))
		    return FALSE;
		break;
	    case '1':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site1))
		    return FALSE;
		break;
	    case '2':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site2))
		    return FALSE;
		break;
	    case '3':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site3))
		    return FALSE;
		break;
	    case '4':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site4))
		    return FALSE;
		break;
	    case '5':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site5))
		    return FALSE;
		break;
	    case '6':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site6))
		    return FALSE;
		break;
	    case '7':
		if (!GetInt32(stream, (unsigned int *)&vv->Versions.Site7))
		    return FALSE;
		break;
	    case 's':
		if (!GetInt32(stream, (unsigned int *)&vv->StoreId.Host))
		    return FALSE;
		break;
	    case 'u':
		if (!GetInt32(stream, (unsigned int *)&vv->StoreId.Uniquifier))
		    return FALSE;
		break;
	    case 'f':
		if (!GetInt32(stream, (unsigned int *)&vv->Flags))
		    return FALSE;
		break;
	}
    }
    if (tag != EOF && tag != (byte)D_ENDVV) {
	LogMsg(0, VolDebugLevel, stdout, "GetVV: Error at end of VV");
	return FALSE;
    }

    return TRUE;
}

/*************************** From readstuff.c */

/* We also need a bogus definition of WriteDump. This should never be called */
long WriteDump(RPC2_Handle _cid, RPC2_Unsigned offset, RPC2_Unsigned *nbytes, RPC2_Unsigned volid, SE_Descriptor *sed)
{
    printf("In WRITEDUMP. should NEVER get here!\n");
    CODA_ASSERT(0);
    return(0); /* to keep C++ happy */
}



/* NOTE: there's gobs of duplication here with readstuff.c. Main problem is
 * that here we need to do seeks() through the stream to find vnodes on the
 * second pass. That doesn't seem necessarily useful in readstuff.
 */

dumpstream::dumpstream(char *filename)
{
  stream = 0; /* default is failure */
  memset(name, 0, sizeof(name));  /* clear name */

/* allow use of null or empty filename to mean stdin; operations involving
     seeks() won't work in that case */
  if ((filename == NULL) || (filename[0] == 0)) {
    stream = stdin;
  }
  else {
    stream = fopen(filename, "r");
    if (stream == NULL) {
	LogMsg(0, VolDebugLevel, stderr, "Can't open dump file %s", filename);
	exit(-1);
    }
    strncpy(name, filename, (sizeof(name)-1));
  }

  IndexType = -1;
}       

dumpstream::~dumpstream(){
  fclose(stream);
  memset(name, 0, sizeof(name));
}

int dumpstream::isopen(){
  if (stream) return (1);
  else return(0);
}

int dumpstream::getDumpHeader(struct DumpHeader *hp)
{
    int tag;
    unsigned int beginMagic;
    if (fgetc(stream) != D_DUMPHEADER
       || !GetInt32(stream, &beginMagic)
       || !GetInt32(stream, (unsigned int *)&hp->version)
       || beginMagic != DUMPBEGINMAGIC)
	return 0;
    hp->volumeId = 0;
    while ((tag = fgetc(stream)) > D_MAX) {
	switch(tag) {
	    case 'v':
	    	if (!GetInt32(stream, (unsigned int *)&hp->volumeId))
		    return 0;
		break;
	    case 'p':
	    	if (!GetInt32(stream, (unsigned int *)&hp->parentId))
		    return 0;
		break;
	    case 'n':
	        GetString(stream, hp->volumeName, (int) sizeof(hp->volumeName));
		break;
	    case 'b':
	        if (!GetInt32(stream, (unsigned int *)&hp->backupDate))
		    return 0;
		break;
	    case 'i' :
		if (!GetInt32(stream, (unsigned int *)&hp->Incremental))
		    return 0;
		break;
	    case 'I' :
		if (!GetInt32(stream, (unsigned int *)&hp->oldest) || !GetInt32(stream, (unsigned int *)&hp->latest))
		    return 0;
		break;
	}
    }
    if (!hp->volumeId) {
	return 0;
    }
    ungetc(tag,stream);
    return 1;
}

    
int dumpstream::getVolDiskData(VolumeDiskData *vol)
{
    int  tag;
    memset((char *)vol, 0, (int) sizeof(*vol));

    if (fgetc(stream) != D_VOLUMEDISKDATA) {
	LogMsg(0, VolDebugLevel, stdout, "Volume header missing from dump %s!\n", name);
	/* Return the appropriate error code. */
	return -5;
    }

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch (tag) {
	    case 'i':
		GetInt32(stream, (unsigned int *)&vol->id);
		break;
	    case 'v':
	        GetInt32(stream, (unsigned int *)&(vol->stamp.version));
		break;
	    case 'n':
		GetString(stream, vol->name, (int) sizeof(vol->name));
		break;
	    case 'P':
		GetString(stream, vol->partition, (int) sizeof(vol->partition));
		break;
	    case 's':
		vol->inService = fgetc(stream);
		break;
	    case '+':
		vol->blessed = fgetc(stream);
		break;
	    case 'u':
		GetInt32(stream, (unsigned int *)&vol->uniquifier);
		break;
	    case 't':
		vol->type = fgetc(stream);
		break;
	    case 'p':
	        GetInt32(stream, (unsigned int *)&vol->parentId);
		break;
	    case 'g':
		GetInt32(stream, (unsigned int *)&vol->groupId);
		break;
	    case 'c':
	        GetInt32(stream, (unsigned int *)&vol->cloneId);
		break;
	    case 'b' :
		GetInt32(stream, (unsigned int *)&vol->backupId);
		break;
	    case 'q':
	        GetInt32(stream, (unsigned int *)&vol->maxquota);
		break;
	    case 'm':
		GetInt32(stream, (unsigned int *)&vol->minquota);
		break;
	    case 'x':
		GetInt32(stream, (unsigned int *)&vol->maxfiles);
		break;
	    case 'd':
	        GetInt32(stream, (unsigned int *)&vol->diskused); /* Bogus:  should calculate this */
		break;
	    case 'f':
		GetInt32(stream, (unsigned int *)&vol->filecount);
		break;
	    case 'l': 
		GetShort(stream, (unsigned short *)&vol->linkcount);
		break;
	    case 'a':
		GetInt32(stream, (unsigned int *)&vol->accountNumber);
		break;
	    case 'o':
	  	GetInt32(stream, (unsigned int *)&vol->owner);
		break;
	    case 'C':
		GetInt32(stream, (unsigned int *)&vol->creationDate);
		break;
	    case 'A':
		GetInt32(stream, (unsigned int *)&vol->accessDate);
		break;
	    case 'U':
	    	GetInt32(stream, (unsigned int *)&vol->updateDate);
		break;
	    case 'E':
	    	GetInt32(stream, (unsigned int *)&vol->expirationDate);
		break;
	    case 'B':
	    	GetInt32(stream, (unsigned int *)&vol->backupDate);
		break;
	    case 'O':
	    	GetString(stream, vol->offlineMessage, (int) sizeof(vol->offlineMessage));
		break;
	    case 'M':
		GetString(stream, vol->motd, (int) sizeof(vol->motd));
		break;
	    case 'W': {
		unsigned int length;
		int i;
    		unsigned int data;
	  	GetInt32(stream, &length);
		for (i = 0; i<(int)length; i++) {
		    GetInt32(stream, &data);
		    if (i < (int)(sizeof(vol->weekUse)/sizeof(vol->weekUse[0])))
			vol->weekUse[i] = data;
		}
		break;
	    }
	    case 'D':
		GetInt32(stream, (unsigned int *)&vol->dayUseDate);
		break;
	    case 'Z':
		GetInt32(stream, (unsigned int *)&vol->dayUse);
		break;
	    case 'V':
		GetVV(stream, &vol->versionvector);
		break;
	}
    }
    ungetc(tag, stream);
    return 0;
}

/* This routine checks that the file argument passed is positioned at the
   magic number marking the end of a dump */
int dumpstream::EndOfDump()
{
    unsigned int magic;

    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    if (fgetc(stream) != D_DUMPEND) {
	LogMsg(0, VolDebugLevel, stderr, "End of dump not found for %s", name);
	return -1;
    }

    GetInt32(stream, &magic);
    if (magic != DUMPENDMAGIC) {
	LogMsg(0, VolDebugLevel, stderr, "Dump Magic Value Incorrect for %s", name);
	return -1;
    }

    if (fgetc(stream) != EOF) {
	LogMsg(0, VolDebugLevel, stderr, "Unrecognized postamble in dump %s", name);
	return -1;
    }

    return 0;
}

int dumpstream::getVnodeIndex(VnodeClass Type, long *nVnodes, long *listsize)
{
    register signed char tag;
    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    if (Type == vLarge) {
	IndexType = vLarge;
	if (fgetc(stream) != D_LARGEINDEX) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR: Large Index not found in %s", name);
	    return -1;
	}
    } else if (Type == vSmall) {
	IndexType = vSmall;
	if (fgetc(stream) != D_SMALLINDEX) {
	    LogMsg(0, VolDebugLevel, stderr, "ERROR: Small Index not found in %s", name);
	    return -1;
	}
    } else {
	LogMsg(0, VolDebugLevel, stderr, "Illegal type passed to GetVnodeIndex!");
	return -1;
    }

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch(tag) {
	  case 'v':
	    if (!GetInt32(stream, (unsigned int *)nVnodes))
		return -1;
	    break;
	  case 's':
	    if (!GetInt32(stream, (unsigned int *)listsize))
		return -1;
	    break;
	  default:
	    LogMsg(0, VolDebugLevel, stderr, "Unexpected field of Vnode found in %s.", name);
	    exit(-1);
	}
    }
    CODA_ASSERT(*listsize > 0);
    ungetc(tag, stream);
    return 0;
}    

/* next byte is either a vnode tag, Index tag, D_FILEDATA or D_DIRPAGES. */
int dumpstream::skip_vnode_garbage()
{
    char buf[4096];
    unsigned int size; 
    
    int tag = fgetc(stream);

    if (tag == D_DIRPAGES) {
	unsigned int npages;
	int size = DIR_PAGESIZE;

	CODA_ASSERT (IndexType == vLarge);
	LogMsg(10, VolDebugLevel, stdout, "SkipVnodeData: Skipping dirpages for %s", name);
	if (!GetInt32(stream, &npages))
	    return -1;
	
	for (unsigned int i = 0; i < npages; i++){ 	/* Skip directory pages */
	    tag = fgetc(stream);
	    if (tag != 'P'){
		LogMsg(0, VolDebugLevel, stderr, "Restore: Dir page does not have a P tag");
		return -1;
	    }
	    GetByteString(stream, (byte *)buf, size);
	} 
    } else if (tag == D_FILEDATA) {
	size = (int) sizeof(buf);
	unsigned int nbytes, filesize;

	CODA_ASSERT (IndexType == vSmall);
	LogMsg(10, VolDebugLevel, stdout, "SkipVnodeData: Skipping file data for %s", name);

	if (!GetInt32(stream, &filesize))
	    return -1;

	for (nbytes = filesize; nbytes; nbytes -= size) { /* Skip the filedata */
	    if (nbytes < size)
		size = nbytes;
	    if (fread(buf, size, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading file from dump %s", name);
		return -1; 
	    }
	}
    } else {
	ungetc(tag, stream); 
    }
    return 0;
}

/* next byte should be a D_DIRPAGES; reads the directory pages and
   constructs in VM a directory representation that can be used by the
   routines in the coddir module 

   Returns 0 on success, -1 on failure of any kind */

int dumpstream::readDirectory(PDirInode *dip){

    int nexttag; 


    if (IndexType != vLarge) {
      LogMsg(0, VolDebugLevel, stderr, "dumpstream::readDirectory() called when IndexType is %d instead of vLarge", IndexType);
      return(-1);
    }
    
    nexttag = fgetc(stream);
    if (nexttag != D_DIRPAGES) {
      LogMsg(0, VolDebugLevel, stderr, "dumpstream::readDirectory()found tag %c when expecting D_DIRPAGES", nexttag);
	return(-1);
    }

    /* Find out how many dir pages there are */
    unsigned int npages = 0; 
    if (!GetInt32(stream, &npages)) return -1;

    /* create directory inode for that many pages */
    *dip = (DirInode *)malloc(sizeof(DirInode));
    CODA_ASSERT (*dip); /* malloc better not fail! */
    memset((void *)*dip, 0, sizeof(DirInode)); /* zeroize */

    /* Read the dir pages in */
    for (unsigned int i = 0; i < npages; i++){
      
      (*dip)->di_pages[i] = malloc(DIR_PAGESIZE);
      CODA_ASSERT((*dip)->di_pages[i]); /* malloc better not fail! */

      nexttag = fgetc(stream);
      if (nexttag != 'P'){
	LogMsg(0, VolDebugLevel, stderr, "dumpstream::readDirectory: Dir page does not have a P tag");
	return -1;
      }
      if (!GetByteString(stream, (byte *) (*dip)->di_pages[i], DIR_PAGESIZE)) {
	LogMsg(0, VolDebugLevel, stderr, "dumpstream::readDirectory: read of dir page #%d of %d pages failed", i, npages);
	return -1;
      }
    }    
    return 0;
}

/*
 * fseek to offset and read in the Vnode there. Assume IndexType is set correctly.
 */
int dumpstream::getVnode(int vnum, long unique, off_t offset, VnodeDiskObject *vdo)
{
    LogMsg(10, VolDebugLevel, stdout, "getVnode: vnum %d unique %d offset %x Stream %s", vnum, unique, offset, name);

    if (name[0] == 0) {
      /* we are using stdin; seek() doesn't work */
      errno = EINVAL;
      return(-1);
    }
    fseeko(stream, offset, 0);	/* Should I calculate the relative? */

    int deleted;
    off_t pos;
    VnodeId vnodeNumber;

    int result = getNextVnode(vdo, &vnodeNumber, &deleted, &pos);

    if (result)
	return result;
    
    LogMsg(10, VolDebugLevel, stdout, "getVnode after getNextVnode: (%x, %d, %lx)", vnodeNumber, deleted, pos);
    CODA_ASSERT(offset == pos);

    CODA_ASSERT(((unsigned int)vnum) == vnodeNumber);	/* They'd better match! */
    CODA_ASSERT(unique == (long)vdo->uniquifier);
    return 0;
}

/*
 * The idea here is to make the dump look like a series of VnodeDiskObjects.
 * Handle the file or directory data associated with Vnodes transparently.
 */

int dumpstream::getNextVnode(VnodeDiskObject *vdop, VnodeId *vnodeNumber, int *deleted, off_t *offset)
{
    *deleted = 0;
    /* Skip over whatever garbage exists on the stream (remains of last vnode) */
    skip_vnode_garbage();

    *offset = ftello(stream);
    /* Now we'd better either be reading a valid or a null Vnode */
    int tag = fgetc(stream); 
    if (tag == D_NULLVNODE){
	vdop->type = vNull;
	return 0;
    } else if (tag == D_RMVNODE) { /* Vnode was deleted */
	if (!GetInt32(stream, (unsigned int *)vnodeNumber) ||
	    !GetInt32(stream, (unsigned int *)&vdop->uniquifier)) 
	    return -1;
	LogMsg(10, VolDebugLevel, stdout, "Deleted Vnode (%x.%x) found ",
	       *vnodeNumber, vdop->uniquifier);
	*deleted = 1;
	return 0;
    } else if (tag != D_VNODE){	   /* Probably read past last vnode in list */
	LogMsg(10, VolDebugLevel, stdout, "Ackk, bogus tag found %c", tag);
	ungetc(tag, stream);
	return -1;
    }

    LogMsg(10, VolDebugLevel, stdout, "GetNextVnode: Real Vnode found! ");
    unsigned int temp1, temp2;
    if (!GetInt32(stream, &temp1) ||
	!GetInt32(stream, &temp2))
	return -1;
    *vnodeNumber = temp1; /* type convert to VnodeId */
    vdop->uniquifier = temp2;
    LogMsg(10, VolDebugLevel, stdout, "%d", *vnodeNumber);

    while ((tag = fgetc(stream)) > D_MAX && tag != EOF) {
	switch (tag) {
	  case 't':
	    vdop->type = (VnodeType) fgetc(stream); 
	    break;
	  case 'b': 
	    short modeBits;
	    GetShort(stream, (unsigned short *)&modeBits);
	    vdop->modeBits = modeBits;
	    break;
	  case 'l':
	    GetShort(stream, &vdop->linkCount);
	    break;
	  case 'L':
	    GetInt32(stream, (unsigned int *)&vdop->length);
	    break;
	  case 'v':
	    GetInt32(stream, (unsigned int *)&vdop->dataVersion);
	    break;
	  case 'V':
	    GetVV(stream, &vdop->versionvector);
	    break;
	  case 'm':
	    GetInt32(stream, (unsigned int *)&vdop->unixModifyTime);
	    break;
	  case 'a':
	    GetInt32(stream, (unsigned int *)&vdop->author);
	    break;
	  case 'o':
	    GetInt32(stream, (unsigned int *)&vdop->owner);
	    break;
	  case 'p':
	    GetInt32(stream, (unsigned int *)&vdop->vparent);
	    break;
	  case 'q':
	    GetInt32(stream, (unsigned int *)&vdop->uparent);
	    break;
	  case 'A':
	    CODA_ASSERT(vdop->type == vDirectory);
	    GetByteString(stream, (byte *)VVnodeDiskACL(vdop), VAclDiskSize(vdop));
	    break;

	  default:
	    break;
	}
    }

    ungetc(tag, stream);
    return 0;
}

/* Copy the file or directory data which should be next in the stream to out */
/* Actually use the bogus Dump mechanism in DumpFD, it should be rewritten soon.*/
extern char *Reserve(DumpBuffer_t *, int);
int
dumpstream::copyVnodeData(DumpBuffer_t *dbuf)
{
    int tag = fgetc(stream);
    char buf[DIR_PAGESIZE];
    unsigned int nbytes;

    LogMsg(10, VolDebugLevel, stdout, "Copy:%s type %x", (IndexType == vLarge?"Large":"Small"), tag);
    if (IndexType == vLarge) {
	CODA_ASSERT(tag == D_DIRPAGES); /* Do something similar for dirpages. */

	/* We get a number of pages, pages are DIR_PAGESIZE bytes long. */
	unsigned int num_pages;
	
	if (!GetInt32(stream, &num_pages))
	    return -1;

	DumpInt32(dbuf, D_DIRPAGES, num_pages);

	for (unsigned int i = 0; i < num_pages; i++) { /* copy DIR_PAGESIZE bytes to output. */
	    tag = fgetc(stream);
	    if (tag != 'P'){
		LogMsg(0, VolDebugLevel, stderr, "Restore: Dir page does not have a P tag");
		return -1;
	    }
	    LogMsg(10, VolDebugLevel, stdout, "copying one page for %s", name);
	    if (fread(buf, DIR_PAGESIZE, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s.", name);
		return -1;
	    }
	    DumpByteString(dbuf, 'P', buf, DIR_PAGESIZE);
	}
    } else if (IndexType == vSmall) {
	/* May not have a file associated with this vnode? don't think so...*/
	CODA_ASSERT(tag == D_FILEDATA);

	/* First need to output the tag and the length */
	unsigned int filesize, size = DIR_PAGESIZE;
	if (!GetInt32(stream, (unsigned int *)&filesize)) 
	    return -1;
	char *p;
	DumpInt32(dbuf, D_FILEDATA, filesize);
	for (nbytes = filesize; nbytes; nbytes -= size) {
	    if (nbytes < size)
		size = nbytes;
	    p = Reserve(dbuf, size);	/* Mark off size bytes in output */
	    if (fread(p, size, 1, stream) != 1) {
		LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s.", name);
		return -1;
	    }
	}
	return filesize;
    } 
    return 0;	    
}

void dumpstream::setIndex(VnodeClass vclass)
{
    CODA_ASSERT((vclass == vLarge) || (vclass == vSmall));
    IndexType = vclass;
}

/* Copy nbytes of file data at current position in dumpstream to membuf;
   assume membuf is big enough; return 0 on success, -1 on failure */
int dumpstream::CopyBytesToMemory(char *membuf, int nbytes) {
  int rc, blobsize, tag;

  /* File data in dumpstream is preceded by tag and size */
  tag = fgetc(stream); 
  if (tag != D_FILEDATA) {
    LogMsg(0, VolDebugLevel, stderr, "Error reading tag from dump file");
    return(-1);
  }

  rc = GetInt32(stream, (unsigned int *)&blobsize);
  if (!rc) {
    LogMsg(0, VolDebugLevel, stderr, "Error reading blob size from dump file");
    return(-1);
  }

  if (blobsize != nbytes) {
    LogMsg(0, VolDebugLevel, stderr, "Size mismatch: expected %d, found %d",
	   nbytes, blobsize);
    return(-1);
  }

  rc = fread(membuf, nbytes, 1, stream);
  if (rc != 1) {
    LogMsg(0, VolDebugLevel, stderr, "Error reading blob data from dump file");
    return(-1);
  }
  return(0);
}

/* Copy nbytes of file data at current position in dumpstream to outfile in
   512-byte records.  Last block is padded to 512 bytes.
   Returns 0 on success, -1 on failure 
*/
int dumpstream::CopyBytesToFile(FILE *outfile, int nbytes) {
  int rc, bytesleft, blobsize, tag;
  char buf[512];

  /* File data in dumpstream is preceded by tag and size */
  tag = fgetc(stream); 
  if (tag != D_FILEDATA) {
    LogMsg(0, VolDebugLevel, stderr, "Error reading tag from dump file");
    return(-1);
  }

  rc = GetInt32(stream, (unsigned int *)&blobsize);
  if (!rc) {
    LogMsg(0, VolDebugLevel, stderr, "Error reading blob size from dump file");
    return(-1);
  }

  if (blobsize != nbytes) {
    LogMsg(0, VolDebugLevel, stderr, "Size mismatch: expected %d, found %d",
	   nbytes, blobsize);
    return(-1);
  }

  for (bytesleft = nbytes; bytesleft >= 512; bytesleft -= 512) {
    rc = fread(buf, 512, 1, stream);
    if (rc != 1) {
      LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s", name);
      return(-1);
    }
    rc = fwrite(buf, 512, 1, outfile);
    if (rc != 1) {
      LogMsg(0, VolDebugLevel, stderr, "Error writing tar file");
      return(-1);
    }

  }

  if (bytesleft) { /* last record! */
    memset(buf, 0, 512);  /* pad with zeros */

    rc = fread(buf, bytesleft, 1, stream);
    if (rc != 1) {
      LogMsg(0, VolDebugLevel, stderr, "Error reading dump file %s", name);
      return(-1);
    }

    rc = fwrite(buf, 512, 1, outfile);
    if (rc != 1) {
      LogMsg(0, VolDebugLevel, stderr, "Error writing tar file.");
      return(-1);
    }
  }

  return(0); /* done! */
}


void PrintDumpHeader(FILE * outfile, struct DumpHeader *dh)
{
    time_t timestamp;

    fprintf(outfile, "Volume id = %08x, Volume name = '%s'\n", 
	 dh->volumeId, dh->volumeName);
    timestamp = (time_t)dh->backupDate;
    fprintf(outfile, "Parent id = %08x  Timestamp = %s", 
	 dh->parentId, ctime(&timestamp));
    fprintf(outfile, "Dump uniquifiers: oldest = %08x   latest = %08x\n",
	 dh->oldest, dh->latest);
}
