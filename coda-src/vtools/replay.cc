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







#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include "coda_string.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#ifdef sun
int utimes(const char *, const struct timeval *);
#endif

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
    int i = 0;
    char *cp;

    for (cp = hdr.dbuf.chksum; cp < &hdr.dbuf.chksum[sizeof(hdr.dbuf.chksum)]; cp++)
	*cp = ' ';

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
#ifdef __CYGWIN32__
     int fd = open(path, O_RDWR);
     if ( fd < 0 )
	    fprintf(stderr, "replay: can't set length on %s: ", path);
     if ( ftruncate(fd, size) != 0 ) {
#else
    if (rflag && truncate(path, size) < 0) {
#endif
	if (vflag || hflag) {
	    fprintf(stderr, "replay: can't set length on %s: ", path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
#ifdef __CYGWIN32__
    close(fd);
#endif
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
