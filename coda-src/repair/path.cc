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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/repair/path.cc,v 4.8 1998/08/31 12:23:17 braam Exp $";
#endif /*_BLURB_*/






/* 
   Routines pertaining to pathname processing for repair tool.
   NONE of these routines have any global side effects.

   Created: M. Satyanarayanan
            October 1989
*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/param.h>
#ifndef __CYGWIN32__
#include <sys/dir.h>
#endif
#include <netinet/in.h>
#include <strings.h>
#include <sys/stat.h>
#include <rpc2.h>
#include <unistd.h>
#include <stdlib.h>

#include <inodeops.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <venusioctl.h>
#include <repio.h>
#include "repair.h"

extern int session;

static char *repair_abspath(char *result, unsigned int len, char *name);
static int repair_getvid(char *, VolumeId *);

/*
 leftmost: check pathname for inconsistent object

 path:	user-provided path of alleged object in conflict
 realpath:	true path (sans sym links) of object in conflict
 
 Returns 0 iff path refers to an object in conflict and this is the
           leftmost such object on its true path (as returned by getwd())
 Returns -1, after printing error messages, in all other cases.  
*/
int repair_isleftmost(char *path, char *realpath, int len)
{
    register char *car, *cdr;
    int symlinks;
    char buf[MAXPATHLEN], symbuf[MAXPATHLEN], here[MAXPATHLEN], tmp[MAXPATHLEN];
    
    DEBUG(("repair_isleftmost(%s, %s)\n", path, realpath));
    strcpy(buf, path); /* tentative */
    symlinks = 0;
    if (!getcwd(here, sizeof(here))) { /* remember where we are */
	printf("Couldn't stat current working directory\n");
	exit(-1);
    }
#define RETURN(x) {assert(!chdir(here)); return(x);}

    /* simulate namei() */
    while (1)
	{
	/* start at beginning of buf */
	if (*buf == '/')
	    {
	    assert(!chdir("/"));
	    car = buf+1;
	    }
	else car = buf;
	
	/* proceed left to right */
	while (1)
	    {
	    /* Lop off next piece */
	    cdr = index(car, '/');
	    if (!cdr)
	    	{/* We're at the end */
		    if (session) {
			repair_abspath(realpath, len, car);
			RETURN(0);
		    } else {
			if (repair_inconflict(car, 0) == 0)
			  {
			      repair_abspath(realpath, len, car);
			      RETURN(0);
			  } else {
			      printf("object not in conflict\n");
			      RETURN(-1);
			  }
		    }
		}
	    *cdr = 0; /* clobber slash */
	    cdr++;

	    /* Is this piece ok? */
	    if (repair_inconflict(car, 0) == 0) {
		printf("%s is to the left of %s and is in conflict\n", 
		       repair_abspath(tmp, MAXPATHLEN, car), path);
		RETURN(-1);
	    }
	    
	    /* Is this piece a sym link? */
	    if (readlink(car, symbuf, MAXPATHLEN) > 0) {
		if (++symlinks >= CODA_MAXSYMLINKS)
		    {
		    errno = ELOOP;
		    perror(path);
		    RETURN(-1);
		    }
		strcat(symbuf, "/");
		strcat(symbuf, cdr);
		strcpy(buf, symbuf);
		break; /* to outer loop, and restart scan */
		}
		
	    /* cd to next component */
	    if (chdir(car) < 0)
		{
		perror(repair_abspath(tmp, MAXPATHLEN, car));
		RETURN(-1);
		}
		
	    /* Phew! Traversed another component! */
	    car = cdr;
	    *(cdr-1) = '/'; /* Restore clobbered slash */
	    }
	}
#undef RETURN
    }
    


/*
    Obtains mount point of last volume in realpath
    Returns 0 on success, -1 on failure.
    Null OUT parameters will not be filled.
       
    realpath:	abs pathname (sans sym links)of replicated object
    prefix:	part before last volume encountered in realpath
    suffix:	part inside last volume
    vid:	id of last volume

    CAVEAT: code assumes realpath has no conflicts except (possibly)
          last component.  This is NOT checked.
*/
int repair_getmnt(char *realpath, char *prefix, char *suffix, VolumeId *vid)
{
    char buf[MAXPATHLEN];
    VolumeId currvid, oldvid;
    char *slash;
    int rc;

    DEBUG(("repair_getmnt(%s...)\n", realpath));

    /* Find abs path */
    assert(*realpath == '/');
    strcpy(buf, realpath);
    
    /* obtain volume id of last component */
    rc = repair_getvid(buf, &currvid);
    if (rc < 0) return(-1);
    

    /* Work backwards, shrinking realpath and testing if we have
       crossed a mount point */
    slash = buf + strlen(buf); /* points at trailing null */
    while (1) {
	    /* INV: chars at and to right of slash have been examined
	       none to left have */
	    char *oldslash = slash;
	    oldvid = currvid;
	    
	    /* break the string and find nex right slash */
	    *oldslash = 0; 
	    slash = strrchr(buf, '/'); 
	    *oldslash = '/';
	    /* abs path ==> '/' guaranteed */
	    assert(slash);

	    if (slash == buf) break; /* ate whole path up */
	    *slash = 0;  

	    rc = repair_getvid(buf, &currvid);
	    if (rc < 0) {
		    if (errno == EINVAL) break; /* crossed out of Coda */
		    perror(buf);  /* a real error */
		    return(-1); 
	    }
	    *slash = '/';  /* restore the nuked component */

	    DEBUG(("oldvid = %ld   currvid = %ld\n", oldvid, currvid));
	    /* crossed an internal Coda mount point */
	    if (oldvid != currvid) {
		    /* restore slash to previous value and break */
		    slash = oldslash; 
		    *slash = 0;  
		    break;
	    }
    }

    /* set OUT parameters */
    if (prefix) 
	    strcpy(prefix, buf);  /* this gives us the mount point */
    if (suffix) {
	    if (strlen(buf) == strlen(realpath))
		*suffix = 0;  /* realpath is the root of a volume */
	else 
		strcpy(suffix, buf+strlen(buf)+1); 
    }
    if (vid) 
	    *vid = oldvid;

    return(0);
}

/* 
   returns 0 if name refers to an object in conflict
   and conflictfid is filled  if it is non-null.
   returns -1 otherwise (even if any other error)
       
   CAVEAT: assumes no conflicts to left of last component.
   This is NOT checked.
*/
int repair_inconflict(char *name, ViceFid *conflictfid /* OUT */)
{
    int rc;
    char symval[MAXPATHLEN];
    struct stat statbuf;

    DEBUG(("repair_inconflict(%s,...)\n", name));

    rc = stat(name, &statbuf);
    if ((rc == 0) || (errno != ENOENT)) 
	    return(-1);
    
    /* is it a sym link? */
    symval[0] = 0;
    rc = readlink(name, symval, MAXPATHLEN);
    if (rc < 0) return(-1);

    /* it's a sym link, alright */
    if (symval[0] == '@')
	{
	if (conflictfid)
		sscanf(symval, "@%lx.%lx.%lx",
		       &conflictfid->Volume, &conflictfid->Vnode, 
		       &conflictfid->Unique);
	return (0);
	}
    else 
	    return(-1);
    }

/* Returns 0 and fills outfid and outvv with fid and version vector
   for specified Coda path.  If version vector is not accessible, the
   StoreId fields of outvv are set to -1.  Garbage may be copied into
   outvv for non-replicated files
	  
   Returns -1 after printing error msg on failures. 
*/
int repair_getfid(char *path, ViceFid *outfid /* OUT */,
		  ViceVersionVector *outvv /* OUT */)
{
    int rc, saveerrno;
    struct ViceIoctl vi;
    char junk[2048];

    DEBUG(("repair_getfid(%s, ...)\n", path));
    vi.in = 0;
    vi.in_size = 0;
    vi.out = junk;
    vi.out_size = sizeof(junk);
    bzero(junk, sizeof(junk));

    rc = pioctl(path, VIOC_GETFID, &vi, 0);
    saveerrno = errno;

    /* Easy: no conflicts */
    if (!rc) {
	bcopy((const char *)junk, (void *)outfid, sizeof(ViceFid));
	bcopy((const char *)junk+sizeof(ViceFid), (void *)outvv, sizeof(ViceVersionVector));
	return(0);
    }

    /* Perhaps the object is in conflict? Get fid from dangling symlink */
    rc = repair_inconflict(path, outfid);
    if (!rc) {
	outvv->StoreId.Host = (u_long) -1;  /* indicates VV is undefined */
	outvv->StoreId.Uniquifier = (u_long)-1;
	return(0);
    }

    /* No: 'twas some other bogosity */
    if (errno != EINVAL)
	repair_perror(" GETFID", path, saveerrno);
    return(-1);
}


/* return 1 when in coda */
int repair_IsInCoda(char *name) 
{
    static char buf[MAXPATHLEN];
    char *tmpname = name;
    if (name[0] != '/') {
       	tmpname = getcwd(buf, MAXPATHLEN);
	assert(tmpname);
    }
    /* test absolute path name */
    if (strncmp(tmpname, "/coda", 5) == 0 ) 
	    return(1);
    else 
	    return (0);
}

static char *repair_abspath(char *result, unsigned int len, char *name)
{
    assert(getcwd(result, len));
    assert( strlen(name) + 1 <= len );

    strcat(result, "/");
    strcat(result, name);
    return(result);
}


/* Returns 0 and fills volid with the volume id of path.  Returns -1
   on failure */

static int repair_getvid(char *path, VolumeId *vid)
{
    ViceFid vfid;
    ViceVersionVector vv;
    int rc;

    DEBUG(("repair_getvid(%s,...)\n", path));

    rc = repair_getfid(path, &vfid, &vv);
    if (rc < 0) return(-1); /* getfid does perror() */
    *vid = vfid.Volume;
    return(0);    
    }

void repair_perror(char *op, char *path, int e)
{
    char msg[MAXPATHLEN+100];
    
    sprintf(msg, "%s: %s", op, path);
    errno = e;  /* in case it has been clobbered by now */
    perror(msg);
}
