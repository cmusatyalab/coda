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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/venus/vproc_pathname.cc,v 4.11 1998/11/11 15:59:01 smarc Exp $";
#endif /*_BLURB_*/







/*
 *
 *    Implementation of the Venus path expansion facility.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* interfaces */
#include <vice.h>

/* from venus */
#include "fso.h"
#include "venus.private.h"
#include "venusrecov.h"
#include "vproc.h"


inline void GetComponent(char **pptr_addr, int *plen_addr, char *nextcompptr) {
    char c;
   
 while ((c = **pptr_addr) && c != '/') {
	/* Move on to next character. */
	*nextcompptr++ = c;
	(*pptr_addr)++;
	(*plen_addr)--;
    }
    *nextcompptr = 0;	    /* make comp a real string */
}


inline void SkipSlashes(char **pptr_addr, int *plen_addr) {
    while (**pptr_addr == '/') {
	(*pptr_addr)++;
	(*plen_addr)--;
    }
}


/* Should be possible to inhibit symlink expansion (i.e., consider it an error)! */
/* Returns {0, 1}.  On 0, u.u_error is set to appropriate Unix errno and *vpp is 0. */
/* On 1, u.u_error is 0 and *vpp is a valid vnode pointer. */
/* Caller must set u_cred, u_priority, u.u_cdir and u_nc fields as appropriate. */
int vproc::namev(char *path, int flags, struct venus_cnode *vpp) {
    LOG(1, ("vproc::namev: %s, %d\n", path, flags));

    /* Initialize some global variables. */
    u.u_error = 0;
    u.u_flags = flags;
    struct venus_cnode pvp;
    struct venus_cnode vp;
    char comp[CODA_MAXNAMLEN];
    comp[0] = '\0';
    char workingpath[CODA_MAXNAMLEN];
    strcpy(workingpath, path);
    char *pptr = workingpath;
    int plen = strlen(pptr);
    int nlinks = 0;

    /* Initialize the parent (i.e., the root of the expansion). */
    {
	struct cfid fid;
	fid.cfid_len = sizeof(ViceFid);
	fid.cfid_fid = u.u_cdir;
	vget(&pvp, &fid);
	if (u.u_error) goto Exit;

	/* Skip over leading slashes. */
	if (plen != 0) SkipSlashes(&pptr, &plen);

	/* Check for degenerate case of asking for Cdir. */
	if (plen == 0) {
	    *vpp = pvp;
	    goto Exit;
	}
    }

    /* Each loop iteration moves down through one pathname component. */
    for (;;) {
	if (plen <= 0)
	    { print(logFile); CHOKE("vproc::namev: plen <= 0"); }

	/* Get the next component. */
	GetComponent(&pptr, &plen, comp);

	/* Skip over trailing slashes. */
	SkipSlashes(&pptr, &plen);

	/* Handle ".." out of venus here! */
	if (FID_EQ(&(pvp.c_fid), &rootfid) && STREQ(comp, "..")) {
	    LOG(100, ("vproc::namev: .. out of this venus\n"));

	    u.u_error = ENOENT;
	    goto Exit;
	}

	/* Now lookup the object in the directory. */
	lookup(&pvp, comp, &vp, CLU_CASE_SENSITIVE);
	if (u.u_error) goto Exit;

	/* We have the new object.  The next action depends on what type of object it is. */
	/* If it is a file, we check that we are at the end of the path; */
	/* If it is a directory, we simply make it the new parent object. */
	/* If it is a symbolic link, we reset the pathname to be it, and continue scanning. */
	switch(vp.c_type) {
	    case C_VREG:
		{
		if (plen == 0) {
			*vpp = vp;
			goto Exit;
		}

		/* File must be the last comp in the path. */
		u.u_error = ENOTDIR;
		goto Exit;
		}

	    case C_VDIR:
		{
		if (plen == 0) {
		    *vpp = vp;
		    goto Exit;
		}

		/* Child becomes the new parent. */
		pvp = vp;
		comp[0] = '\0';

		break;
		}

	    case C_VLNK:
		{
		/* Return the link if we are not to "follow" and this is the last component. */
		if (plen == 0 && !(u.u_flags & FOLLOW_SYMLINKS)) {
		    *vpp = vp;
		    goto Exit;
		}

		/* Guard against looping. */
		if (++nlinks > CODA_MAXSYMLINK) {
		    u.u_error = ELOOP;
		    goto Exit;
		}

		/* Examine the link contents.  We will find either: */
		/*    - an absolute pathname within this venus; */
		/*    - an absolute pathname leaving this venus; */
		/*    - a relative pathname. */
		/* Note that the result of ReadLink is not necessarily a proper string. */

		/* Get the link contents. */
		char linkdata[MAXPATHLEN];
		int linklen = MAXPATHLEN;
		struct coda_string string;
		string.cs_buf = linkdata;
		string.cs_maxlen = linklen;
		string.cs_len = 0;
		readlink(&vp, &string);
		if (u.u_error) {
			linklen = 0;
			goto Exit;
		}
		linklen = string.cs_len;
		if (linklen == 0) {
		    u.u_error = EINVAL;
		    goto Exit;
		}

		/* Append the trailing part of the original pathname. */
		if (linklen + plen > MAXPATHLEN) {
		    u.u_error = ENAMETOOLONG;
		    goto Exit;
		}
		if (plen > 0) {
		    linkdata[linklen] = '/';
		    linkdata[linklen + 1] = '\0';
		    strcat(linkdata, pptr);
		}
		else
		    linkdata[linklen] = '\0';

		/* Figure out the type of the new path and act accordingly. */
		static int venusRootLength = -1;
		if (venusRootLength == -1)
		    venusRootLength = strlen(venusRoot);
		if (linklen >= venusRootLength &&
		    STRNEQ(linkdata, venusRoot, venusRootLength) &&
		    (linklen == venusRootLength || linkdata[venusRootLength] == '/')) {
		    LOG(100, ("vproc::namev: abspath within this venus (%s)\n", linkdata));

		    /* Copy the part after the VenusRoot to workingpath. */
		    strcpy(workingpath, linkdata + venusRootLength);

		    /* Release the child. */
		    comp[0] = '\0';

		    /* Release the parent and reset it to the VenusRoot. */
		    struct cfid fid;
		    fid.cfid_len = sizeof(ViceFid);
		    fid.cfid_fid = rootfid;
		    vget(&pvp, &fid);
		    if (u.u_error) goto Exit;
		}
		else if (linkdata[0] == '/') {
		    LOG(100, ("vproc::namev: abspath leaving this venus (%s)\n", linkdata));

		    u.u_error = ENOENT;
		    goto Exit;
		}
		else {
		    LOG(100, ("vproc::namev: relpath (%s)\n", linkdata));

		    /* Copy the whole path to workingpath. */
		    strcpy(workingpath, linkdata);

		    /* Release the child. */
		    comp[0] = '\0';

		    /* Parent stays the same. */
		}

		/* Reset the name pointers. */
		pptr = workingpath;
		plen = strlen(pptr);

		/* Skip over leading slashes. */
		SkipSlashes(&pptr, &plen);

		/* Check (again) for degenerate case of asking for root. */
		if (plen == 0) {
		    *vpp = pvp;
		    goto Exit;
		}

		break;
		}

	    default:
		{
		print(logFile);
		CHOKE("vproc::namev: bogus vnode type (%d)!", vp.c_type);
		}
	}
    }

Exit:
    LOG(1, ("vproc::namev: returns %s\n", VenusRetStr(u.u_error)));
    return(u.u_error == 0);
}


/* Map fid to full or volume-relative pathname.  Kind of like getwd(). */
/* XXX - Need fsobj::IsRealRoot predicate which excludes "fake" roots! -JJK */
void vproc::GetPath(ViceFid *fid, char *out, int *outlen, int fullpath) {
    LOG(1, ("vproc::GetPath: %s, %d\n", FID_(fid), fullpath));

    if (*outlen < MAXPATHLEN)
	{ u.u_error = ENAMETOOLONG; *outlen = 0; goto Exit; }

    /* Handle degenerate case of file system or volume root! */
    if (FID_IsVolRoot(fid)) {
	if (!fullpath) {
	    strcpy(out, ".");
	    *outlen = 2;
	    goto Exit;
	}

	if (fid->Volume == rootfid.Volume) {
	    strcpy(out, venusRoot);
	    *outlen = strlen(venusRoot) + 1;
	    goto Exit;
	}
    }

    for (;;) {
	fsobj *f = 0;
	ViceFid currFid;
	ViceFid prevFid;
	out[0] = '\0';
	*outlen = 0;

	Begin_VFS(fid->Volume, CODA_VGET);
	if (u.u_error) goto Exit;

	/* Initialize the "prev" and "current" fids to the target object and its parent, respectively. */
	u.u_error = FSDB->Get(&f, fid, CRTORUID(u.u_cred), RC_STATUS);
	if (u.u_error) goto FreeLocks;
	if (FID_IsVolRoot(&f->fid)) {
	    fsobj *newf = f->u.mtpoint;
	    FSDB->Put(&f);

	    f = newf;
	    if (f == 0) {
		u.u_error = ENOENT;
		goto Exit;
	    }
	    f->Lock(RD);
	}
	prevFid = f->fid;
	currFid = f->pfid;
	FSDB->Put(&f);

	for (;;) {
	    /* Get the current object. */
	    u.u_error = FSDB->Get(&f, &currFid, CRTORUID(u.u_cred), RC_DATA);
	    if (u.u_error) goto FreeLocks;

	    /* Lookup the name of the previous component. */
	    char comp[CODA_MAXNAMLEN];
	    u.u_error = f->dir_LookupByFid(comp, &prevFid);
	    if (u.u_error) goto FreeLocks;

	    /* Prefix the pathname with this latest component. */
	    /* I know this is inefficient; I don't care! -JJK */
	    {
		char tbuf[MAXPATHLEN];
		strcpy(tbuf, out);
		strcpy(out, comp);
		if (tbuf[0] != '\0') {
		    strcat(out, "/");
		    strcat(out, tbuf);
		}
	    }

	    /* Termination condition is when current object is root. */
	    if (FID_IsVolRoot(&f->fid)) {
		if (!fullpath) {
		    char tbuf[MAXPATHLEN];
		    strcpy(tbuf, out);
		    strcpy(out, "./");
		    strcat(out, tbuf);
		    *outlen = strlen(out) + 1;

		    goto FreeLocks;
		}

		if (f->fid.Volume == rootfid.Volume) {
		    char tbuf[MAXPATHLEN];
		    strcpy(tbuf, out);
		    strcpy(out, venusRoot);
		    strcat(out, "/");
		    strcat(out, tbuf);
		    *outlen = strlen(out) + 1;

		    goto FreeLocks;
		}
	    }

	    /* Move on to next object. */
	    if (FID_IsVolRoot(&f->fid)) {
		fsobj *newf = f->u.mtpoint;
		FSDB->Put(&f);

		f = newf;
		if (f == 0) {
		    u.u_error = ENOENT;
		    goto FreeLocks;
		}
		f->Lock(RD);
	    }
	    prevFid = f->fid;
	    currFid = f->pfid;
	    FSDB->Put(&f);
	}

FreeLocks:
	FSDB->Put(&f);
	int retry_call = 0;
	End_VFS(&retry_call);
	if (!retry_call) break;
    }

Exit:
    if (u.u_error != 0) {
	out[0] = '\0';
	*outlen = 0;
    }
    LOG(1, ("vproc::GetPath: returns %s (%s)\n",
	     VenusRetStr(u.u_error), out));
}
