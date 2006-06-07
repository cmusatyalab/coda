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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdint.h>
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
#endif

#include "coda_replay.h"

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
void usage(void);

static char *EXE;

int main(int argc, char *argv[])
{
    EXE = argv[0];

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
		    fprintf(stderr, "%s: s option is deprecated\n", EXE);
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
		fprintf(stderr, "%s: couldn't open %s for reading\n",
			EXE, argv[2]);
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
		fprintf(stderr, "%s: failed header validation\n", EXE);
		exit(-1);
	    }

	    HandleRecord(hdr);
	}
    }
}


int ValidateHeader(hblock& hdr)
{
    uint16_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t mtime;
    uint32_t val;

    if (hdr.dbuf.name[0] == '\0') {
	trailers++;
	return(1);
    }

    sscanf(hdr.dbuf.mode, "%o", &val); mode = val;
    sscanf(hdr.dbuf.uid, "%o", &val); uid = val;
    sscanf(hdr.dbuf.gid, "%o", &val); gid = val;
    sscanf(hdr.dbuf.size, "%o", &val); size = val;
    sscanf(hdr.dbuf.mtime, "%o", &val); mtime = val;

    switch(hdr.dbuf.linkflag) {
	case STOREDATA:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: StoreData(%s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case LINK:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: Link(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case SYMLINK:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: SymLink(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid,
			size, mtime);
	    }
	    break;

	case STORESTATUS:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: StoreStatus(%s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case REMOVE:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: Remove(%s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case RENAME:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: Rename(%s, %s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.linkname, hdr.dbuf.name, mode, uid, gid,
			size, mtime);
	    }
	    break;

	case MKDIR:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: Mkdir(%s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	case RMDIR:
	    if (tflag || vflag) {
		fprintf(stderr, "%s: Rmdir(%s) [ %o, %d, %d, %lu, %lu ]\n",
			EXE, hdr.dbuf.name, mode, uid, gid, size, mtime);
	    }
	    break;

	default:
	    fprintf(stderr, "%s: bogus linkflag %c\n", EXE, hdr.dbuf.linkflag);
	    exit(-1);
    }
    
    uint32_t chksum1, chksum2;
    sscanf(hdr.dbuf.chksum, "%o", &chksum1);
    chksum2 = checksum(hdr);
    sprintf(hdr.dbuf.chksum, "%o", chksum1);
    if (chksum1 != chksum2) {
	fprintf(stderr, "%s: checksum error (%d != %d)\n", EXE, chksum1, chksum2);
	return(0);
    }

    return(1);
}


void HandleRecord(hblock& hdr)
{
    uint16_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t mtime;
    uint32_t val;

    if (hdr.dbuf.name[0] == '\0') {
	/* trailer */
	return;
    }

    sscanf(hdr.dbuf.mode, "%o", &val); mode = val;
    sscanf(hdr.dbuf.uid, "%o", &val); uid = val;
    sscanf(hdr.dbuf.gid, "%o", &val); gid = val;
    sscanf(hdr.dbuf.size, "%o", &val); size = val;
    sscanf(hdr.dbuf.mtime, "%o", &val); mtime = val;

    switch(hdr.dbuf.linkflag) {
	case STOREDATA:
	    {
	    makeprefix(hdr.dbuf.name);
	    if (rflag && freopen(hdr.dbuf.name, "w+", stdout) == NULL) {
		fprintf(stderr, "%s: could not create %s\n",
			EXE, hdr.dbuf.name);
		if (hflag) exit(-1);
		break;
	    }
	    for (; size > 0; size -= TBLOCK) {
		hblock data;
		readblock(data);
		writeblock(data);
	    }
	    if (rflag && fclose(stdout) == EOF) {
		fprintf(stderr, "%s: could not close %s\n", EXE, hdr.dbuf.name);
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
		fprintf(stderr, "%s: can't link %s to %s: ",
			EXE, hdr.dbuf.name, hdr.dbuf.linkname);
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
		fprintf(stderr, "%s: can't symbolic link %s to %s: ",
			EXE, hdr.dbuf.name, hdr.dbuf.linkname);
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
		    fprintf(stderr, "%s: unlink %s failed: ",
			    EXE, hdr.dbuf.name);
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
		    fprintf(stderr, "%s: rename %s to %s failed: ",
			    EXE, hdr.dbuf.linkname, hdr.dbuf.name);
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
		    fprintf(stderr, "%s: mkdir %s failed: ",
			    EXE, hdr.dbuf.name);
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
		    fprintf(stderr, "%s: rmdir %s failed: ",
			    EXE, hdr.dbuf.name);
		    perror("");
		}
		if (hflag) exit(-1);
	    }
	    break;
	    }

	default:
	    fprintf(stderr, "%s: bogus linkflag %c\n", EXE, hdr.dbuf.linkflag);
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
	    fprintf(stderr, "%s: can't set mode on %s: ", EXE, path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
}


void setowner(char *path, int uid, int gid) {
    if (rflag && chown(path, uid, gid) < 0) {
	if (vflag || hflag) {
	    fprintf(stderr, "%s: can't set owner on %s: ", EXE, path);
	    perror("");
	}
	if (hflag) exit(-1);
    }
}


void setlength(char *path, off_t size) {
#ifdef __CYGWIN32__
     int fd = open(path, O_RDWR);
     if ( fd < 0 )
	    fprintf(stderr, "%s: can't set length on %s: ", EXE, path);
     if ( ftruncate(fd, size) != 0 ) {
#else
    if (rflag && truncate(path, size) < 0) {
#endif
	if (vflag || hflag) {
	    fprintf(stderr, "%s: can't set length on %s: ", EXE, path);
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
		fprintf(stderr, "%s: can't set time on %s: ", EXE, path);
		perror("");
	    }
	    if (hflag) exit(-1);
	}
    }
}


void readblock(hblock& blk) {
    if (fread((char *)&blk, sizeof(hblock), 1, stdin) != 1) {
	fprintf(stderr, "%s: fread failed\n", EXE);
	exit(-1);
    }
}


void writeblock(hblock& blk) {
    if (rflag && fwrite((char *)&blk, sizeof(hblock), 1, stdout) != 1) {
	fprintf(stderr, "%s: fwrite failed\n", EXE);
	exit(-1);
    }
}

void usage(void)
{
    fprintf(stderr, "Usage: %s [rstvh] [filename]\n", EXE);
    exit(-1);
}
