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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/vtools/replay.cc,v 1.1 1996/11/22 19:14:22 braam Exp $";
#endif /*_BLURB_*/







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <libc.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "replay.h"

/* Key letters are: */
/*     r  :  replay non-tar commands and pass others to tar */
/*     s  :  strip out but do not replay non-tar commands; ---DEPRECATED--- */
/*     t  :  list, but do not execute, both tar and non-tar commands */
/*     v  :  be verbose */
/*     h  :  be harsh in replaying (i.e., abort the program if any command fails) */


int rflag = 0;
int sflag = 0;
int tflag = 0;
int vflag = 0;
int hflag = 0;
int trailers = 0;

int ValidateHeader(hblock&);
void HandleRecord(hblock&);
int checksum(hblock&);
void makeprefix(char *);
void setmode(char *, int);
void setowner(char *, int, int);
void setlength(char *, off_t);
void settimes(char *, time_t);
void readblock(hblock&);
void writeblock(hblock&);
void usage();


main(int argc, char *argv[]) {
    /* Parse Args. */
    {
	if (argc != 2 && argc != 3)
	    usage();

	for (int i = 0; argv[1][i]; i++)
	    switch(argv[1][i]) {
		case 'r':
		    rflag++;
		    break;

		case 's':
		    fprintf(stderr, "replay: s option is deprecated\n");
		    exit(-1);
		    sflag++;
		    break;

		case 't':
		    tflag++;
		    break;

		case 'v':
		    vflag++;
		    break;

		case 'h':
		    hflag++;
		    break;

		default:
		    usage();
	    }
	if (rflag + sflag + tflag != 1)
	    usage();

	if (argc == 3) {
	    if (freopen(argv[2], "r", stdin) == NULL) {
		fprintf(stderr, "replay: couldn't open %s for reading\n", argv[2]);
		exit(-1);
	    }
	}
    }

    /* Cycle through the input. */
    {
	hblock hdr;
	for (; trailers < 2;) {
	    readblock(hdr);

	    if (!ValidateHeader(hdr)) {
		fprintf(stderr, "replay: failed header validation\n");
		exit(-1);
	    }

	    HandleRecord(hdr);
	}
    }
}


int ValidateHeader(hblock& hdr) {
    if (hdr.dbuf.name[0] == '\0') {
	trailers++;
	return(1);
    }

    /*unsigned short*/int mode;
    sscanf(hdr.dbuf.mode, "%o", &mode);
    /*uid_t*/int uid;
    sscanf(hdr.dbuf.uid, "%o", &uid);
    /*gid_t*/int gid;
    sscanf(hdr.dbuf.gid, "%o", &gid);
    off_t size;
    sscanf(hdr.dbuf.size, "%lo", &size);
    time_t mtime;
    sscanf(hdr.dbuf.mtime, "%lo", &mtime);

    switch(hdr.dbuf.linkflag) {
	case STOREDATA:
	    if (tflag || vflag) {
		fprintf(stderr, "StoreData(%s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case LINK:
	    if (tflag || vflag) {
		fprintf(stderr, "Link(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case SYMLINK:
	    if (tflag || vflag) {
		fprintf(stderr, "SymLink(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case STORESTATUS:
	    if (tflag || vflag) {
		fprintf(stderr, "StoreStatus(%s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case REMOVE:
	    if (tflag || vflag) {
		fprintf(stderr, "Remove(%s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case RENAME:
	    if (tflag || vflag) {
		fprintf(stderr, "Rename(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case MKDIR:
	    if (tflag || vflag) {
		fprintf(stderr, "Mkdir(%s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case RMDIR:
	    if (tflag || vflag) {
		fprintf(stderr, "Rmdir(%s) [ %o, %d, %d, %lu, %lu ]\n",
			hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	default:
	    fprintf(stderr, "replay: bogus linkflag %c\n", hdr.dbuf.linkflag);
	    exit(-1);
    }
    
    int chksum1, chksum2;
    sscanf(hdr.dbuf.chksum, "%o", &chksum1);
    chksum2 = checksum(hdr);
    sprintf(hdr.dbuf.chksum, "%o", chksum1);
    if (chksum1 != chksum2) {
	fprintf(stderr, "replay: checksum error (%d != %d)\n", chksum1, chksum2);
	return(0);
    }

    return(1);
}


void HandleRecord(hblock& hdr) {
    if (hdr.dbuf.name[0] == '\0') {
	/* trailer */
	return;
    }

    /*unsigned short*/int mode;
    sscanf(hdr.dbuf.mode, "%o", &mode);
    /*uid_t*/int uid;
    sscanf(hdr.dbuf.uid, "%o", &uid);
    /*gid_t*/int gid;
    sscanf(hdr.dbuf.gid, "%o", &gid);
    off_t size;
    sscanf(hdr.dbuf.size, "%lo", &size);
    time_t mtime;
    sscanf(hdr.dbuf.mtime, "%lo", &mtime);

    switch(hdr.dbuf.linkflag) {
	case STOREDATA:
	    {
	    makeprefix(hdr.dbuf.name);
	    if (rflag && freopen(hdr.dbuf.name, "w+", stdout) == NULL) {
		fprintf(stderr, "replay: could not create %s\n", hdr.dbuf.name);
		if (hflag) exit(-1);
		break;
	    }
	    for (; size > 0; size -= TBLOCK) {
		hblock data;
		readblock(data);
		writeblock(data);
	    }
	    if (rflag && fclose(stdout) == EOF) {
		fprintf(stderr, "replay: could not close %s\n", hdr.dbuf.name);
		exit(-1);
	    }

	    setmode(hdr.dbuf.name, mode & 07777);
	    setowner(hdr.dbuf.name, uid, gid);
	    settimes(hdr.dbuf.name, mtime);

	    break;
	    }

	case LINK:
	    {
	    makeprefix(hdr.dbuf.name);
	    if (rflag && rmdir(hdr.dbuf.name) < 0) {
		if (errno == ENOTDIR)
		    unlink(hdr.dbuf.name);
	    }
	    if (rflag && link(hdr.dbuf.linkname, hdr.dbuf.name) < 0) {
		fprintf(stderr, "replay: can't link %s to %s: ",
			hdr.dbuf.name, hdr.dbuf.linkname);
		perror("");
		if (hflag) exit(-1);
	    }
	    break;
	    }

	case SYMLINK:
	    {
	    makeprefix(hdr.dbuf.name);
	    if (rflag && rmdir(hdr.dbuf.name) < 0) {
		if (errno == ENOTDIR)
		    unlink(hdr.dbuf.name);
	    }
	    if (rflag && symlink(hdr.dbuf.linkname, hdr.dbuf.name) < 0) {
		fprintf(stderr, "replay: can't symbolic link %s to %s: ",
			hdr.dbuf.name, hdr.dbuf.linkname);
		perror("");
		if (hflag) exit(-1);
	    }
	    break;
	    }

	case STORESTATUS:
	    {
	    setmode(hdr.dbuf.name, mode);
	    setowner(hdr.dbuf.name, uid, gid);
	    setlength(hdr.dbuf.name, size);
	    settimes(hdr.dbuf.name, mtime);
	    break;
	    }

	case REMOVE:
	    {
	    if (rflag && unlink(hdr.dbuf.name) < 0) {
		if (vflag || hflag) {
		    fprintf(stderr, "replay: unlink %s failed: ", hdr.dbuf.name);
		    perror("");
		}
		if (hflag) exit(-1);
	    }
	    break;
	    }

	case RENAME:
	    {
	    if (rflag && rename(hdr.dbuf.linkname, hdr.dbuf.name) < 0) {
		if (vflag || hflag) {
		    fprintf(stderr, "replay: rename %s to %s failed: ",
			    hdr.dbuf.linkname, hdr.dbuf.name);
		    perror("");
		}
		if (hflag) exit(-1);
	    }
	    break;
	    }

	case MKDIR:
	    {
	    if (rflag && mkdir(hdr.dbuf.name, mode) < 0) {
		if (vflag || hflag) {
		    fprintf(stderr, "replay: mkdir %s failed: ", hdr.dbuf.name);
		    perror("");
		}
		if (hflag) exit(-1);
	    }
	    break;
	    }

	case RMDIR:
	    {
	    if (rflag && rmdir(hdr.dbuf.name) < 0) {
		if (vflag || hflag) {
		    fprintf(stderr, "replay: rmdir %s failed: ", hdr.dbuf.name);
		    perror("");
		}
		if (hflag) exit(-1);
	    }
	    break;
	    }

	default:
	    fprintf(stderr, "replay: bogus linkflag %c\n", hdr.dbuf.linkflag);
	    exit(-1);
    }
}


int checksum(hblock& hdr) {
    for (char *cp = hdr.dbuf.chksum; cp < &hdr.dbuf.chksum[sizeof(hdr.dbuf.chksum)]; cp++)
	*cp = ' ';

    int i = 0;
    for (cp = hdr.dummy; cp < &hdr.dummy[TBLOCK]; cp++)
	i += *cp;

    return(i);
}


/* Make all directories needed by name. */
void makeprefix(char *name) {
    if (rflag) {
	register char *cp;

	/* Quick check for existence of directory. */
	if ((cp = rindex(name, '/')) == 0)
	    return;
	*cp = '\0';
	if (access(name, 0)	== 0) {	/* already exists */
	    *cp = '/';
	    return;
	}
	*cp = '/';

	/* No luck, try to make all directories in path. */
	for (cp = name; *cp; cp++) {
	    if (*cp != '/')
		continue;

	    *cp = '\0';
	    if (access(name, 0) < 0) {
		if (mkdir(name, 0777) < 0) {
		    if (vflag || hflag)
			perror(name);
		    if (hflag) exit(-1);
		    *cp = '/';
		    return;
		}
	    }
	    *cp = '/';
	}
    }
}


void setmode(char *path, int mode) {
    if (rflag && chmod(path, mode) < 0) {
	if (vflag || hflag) {
	    fprintf(stderr, "replay: can't set mode on %s: ", path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
}


void setowner(char *path, int uid, int gid) {
    if (rflag && chown(path, uid, gid) < 0) {
	if (vflag || hflag) {
	    fprintf(stderr, "replay: can't set owner on %s: ", path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
}


void setlength(char *path, off_t size) {
    if (rflag && truncate(path, size) < 0) {
	if (vflag || hflag) {
	    fprintf(stderr, "replay: can't set length on %s: ", path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
}


void settimes(char *path, time_t mt) {
    if (rflag) {
	struct timeval tv[2];
	tv[0].tv_sec = time((time_t *) 0);
	tv[1].tv_sec = mt;
	tv[0].tv_usec = tv[1].tv_usec = 0;
	if (utimes(path, tv) < 0) {
	    if (vflag || hflag) {
		fprintf(stderr, "replay: can't set time on %s: ", path);
		perror("");
	    }
	    if (hflag) exit(-1);
	}
    }
}


void readblock(hblock& blk) {
    if (fread((char *)&blk, sizeof(hblock), 1, stdin) != 1) {
	fprintf(stderr, "fread failed\n");
	exit(-1);
    }
}


void writeblock(hblock& blk) {
    if (rflag && fwrite((char *)&blk, sizeof(hblock), 1, stdout) != 1) {
	fprintf(stderr, "fwrite failed\n");
	exit(-1);
    }
}


void usage() {
    fprintf(stderr, "Usage: replay [rstvh] [filename]\n");
    exit(-1);
}
