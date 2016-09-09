/* BLURB gpl

                           Coda File System
                              Release 6

	  Copyright (c) 2008-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

			Additional copyrights
			    none currently
#*/

/* write various archive formats */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <lwp/lwp.h>
#include <coda_assert.h>
#include "archive.h"

int archive_type = CPIO_NEWC;

#define BLOCKSIZE 512 /* both tar and cpio tend to use 512 byte blocks */

union tarheaderblock {
    struct {
	char name[100];
	char mode[8];
	char userid[8];
	char groupid[8];
	char filesize[12];
	char timestamp[12];
	char checksum[8];
	char linkflag;
	char linkname[100];
	/* ustar extension */
	char magic[6];
	char version[2];
	char username[32];
	char groupname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
    } hdr;
    unsigned char data[BLOCKSIZE];
};

/* pad to multiple of block size */
static int write_padding(FILE *fp, size_t align)
{
    char zeros[4096];
    off_t len;

    len = ftello(fp);
    len %= align;
    if (len) {
	memset(zeros, 0, sizeof(zeros));
	if (fwrite(zeros, align - len, 1, fp) != 1)
	    return ENOSPC;
    }
    return 0;
}

int archive_write_entry(FILE *fp, ino_t inode, mode_t mode, uid_t uid,
			nlink_t nlink, time_t mtime, size_t filesize,
			const char *name, const char *linkname)
{
    union tarheaderblock tar;
    size_t namesize;
    unsigned int i, checksum = 0;
    int n, err = 0;
    const char *prefix;

    /* strip leading '/' */
    if (name[0] == '/') name = &name[1];
    namesize = strlen(name) + 1;

    switch (archive_type) {
    case TAR_TAR: /* The old (BSD4.3) tar format */
    case TAR_USTAR: /* The POSIX.1 tar format */
	memset(&tar, 0, sizeof(tar));

	prefix = name;
	if (namesize >= 100) {
	    name = strrchr(name, '/');
	    if (name) {
		namesize = strlen(name);
		name++;
	    }
	    if (archive_type == TAR_TAR || !name || namesize >= 100 ||
		(name - prefix) >= 155)
		return ENAMETOOLONG;
	}

	strcpy(tar.hdr.name, name);
	if (S_ISDIR(mode))
	    strcat(tar.hdr.name, "/");

	sprintf(tar.hdr.mode, "%07o", mode & ~S_IFMT);

	CODA_ASSERT(uid < 0777777UL);
	sprintf(tar.hdr.userid, "%07o", uid);
	sprintf(tar.hdr.groupid, "%07o", 65534);

	if (S_ISLNK(mode)) filesize = 0;
	sprintf(tar.hdr.filesize, "%011llo", (unsigned long long)filesize);
	sprintf(tar.hdr.timestamp, "%011llo", (unsigned long long)mtime);
	memset(&tar.hdr.checksum, ' ', sizeof(tar.hdr.checksum));

	if (S_ISREG(mode) && linkname) /* hard link */
				tar.hdr.linkflag = '1';
	else if (S_ISREG(mode))	tar.hdr.linkflag = '0';
	else if (S_ISDIR(mode)) tar.hdr.linkflag = '5';
	else if (S_ISLNK(mode)) tar.hdr.linkflag = '2';
	else CODA_ASSERT(0 && "unknown fso type");

	if (linkname) {
	    CODA_ASSERT(strlen(linkname) < 100);
	    sprintf(tar.hdr.linkname, "%s", linkname);
	}

	if (archive_type == TAR_USTAR) {
	    strcpy(tar.hdr.magic, "ustar");
	    tar.hdr.version[0] = tar.hdr.version[1] = '0';
	    sprintf(tar.hdr.devmajor, "%07o", 0xC0DA);
	    sprintf(tar.hdr.devminor, "%07o", 0xC0DA);
	    strncpy(tar.hdr.prefix, prefix, name - prefix);
	}

	for (i = 0; i < sizeof(tar); i++)
	    checksum += tar.data[i];
	snprintf(tar.hdr.checksum, 7, "%06o", checksum);

	if (fwrite(&tar, sizeof(tar), 1, fp) != 1)
	    return ENOSPC;
	break;

    case CPIO_ODC: /* The old (POSIX.1) portable format */
	CODA_ASSERT((mode     & ~(mode_t)0777777) == 0);
	CODA_ASSERT((nlink    & ~(nlink_t)0777777) == 0);
	CODA_ASSERT((namesize & ~(size_t)0777777) == 0);
	CODA_ASSERT((filesize & ~(size_t)077777777777ULL) == 0);

	/* when the uid overflows, force it to nobody */
	/* handle overflows by truncating or by using a default value */
	if (inode > 0777777UL)	inode &= 0777777UL;	/* wrap around */
	if (uid > 0777777UL)	uid   = 65534;		/* nobody */
	if ((uint64_t)mtime & ~077777777777ULL)
	    mtime = (time_t)077777777777ULL;		/* truncate to max */

	n = fprintf(fp,
		    "070707"		/* magic, must be "070707" */
		    "140332"		/* device, octal(0xC0DA) */
		    "%06o%06o%06o"	/* inode, mode, uid */
		    "177776"		/* gid, 65534/nogroup/nfsnobody */
		    "%06o"		/* nlink */
		    "000000"		/* rdev, only used by chr and blk dev */
		    "%011llo%06o%011llo"/* mtime, namesize, filesize */
		    "%s%c",		/* name */
		    (uint32_t)inode, (uint32_t)mode, (uint32_t)uid,
		    (uint32_t)nlink, (unsigned long long)mtime, (uint32_t)namesize,
		    (unsigned long long)filesize, name, '\0');
	if (n == -1) return ENOSPC;

	if (S_ISLNK(mode) && fwrite(linkname, filesize, 1, fp) != 1)
	    return ENOSPC;
	break;

    case CPIO_NEWC: /* The new (SVR4) portable format */
	CODA_ASSERT((filesize & ~0xFFFFFFFFULL) == 0);
	if ((uint64_t)mtime & ~0xFFFFFFFFULL)
	    mtime = (uint32_t)0xFFFFFFFFULL; /* truncate to max */
	n = fprintf(fp,
		    "070701"		/* magic, must be "070701" */
		    "%08X%08X%08X"	/* inode, mode, userid */
		    "0000FFFE"		/* groupid, 65534/nogroup/nfsnobody */
		    "%08X%08X%08X"	/* nlink, timestamp, filesize */
		    "C0DAC0DAC0DAC0DA"	/* dev_major, dev_minor */
		    "0000000000000000"	/* rdev_major, rdev_minor */
		    "%08X"		/* namelen including trailing '\0' */
		    "00000000"		/* checksum */
		    "%s%c",		/* name */
		    (uint32_t)inode, (uint32_t)mode, (uint32_t)uid,
		    (uint32_t)nlink, (uint32_t)mtime, (uint32_t)filesize,
		    (uint32_t)namesize, name, '\0');
	if (n == -1) return ENOSPC;

	err = write_padding(fp, sizeof(uint32_t));
	if (err) return err;

	if (S_ISLNK(mode) && fwrite(linkname, filesize, 1, fp) != 1)
	    return ENOSPC;

	return write_padding(fp, sizeof(uint32_t));
    }
    return 0;
}

int archive_write_data(FILE *fp, const char *container)
{
    char buf[4096];
    int in = open(container, O_RDONLY);
    int loop = 0;
    ssize_t n;
    CODA_ASSERT(in != -1);

    while (1) {
	n = read(in, buf, sizeof(buf));
	if (n == -1) return EIO;

	if (n && fwrite(buf, n, 1, fp) != 1)
	    return ENOSPC;

	if ((size_t)n < sizeof(buf)) break;

	/* yield occasionally */
	if (++loop % 32 == 0) {
	    IOMGR_Poll();
	    LWP_DispatchProcess();
	}
    }
    close(in);

    switch (archive_type) {
    case TAR_TAR:
    case TAR_USTAR:
	return write_padding(fp, BLOCKSIZE);

    case CPIO_NEWC:
	return write_padding(fp, sizeof(uint32_t));

    default:
	break;
    }
    return 0;
}

int archive_write_trailer(FILE *fp)
{
    char zeros[1024];
    int err;

    switch (archive_type) {
    case TAR_TAR:
    case TAR_USTAR:
	/* tar trailer is 1024 bytes of '\0' characters */
	memset(zeros, 0, sizeof(zeros));
	if (fwrite(zeros, sizeof(zeros), 1, fp) != 1)
	    return ENOSPC;
	break;

    case CPIO_ODC:
    case CPIO_NEWC:
	/* write end record */
	err = archive_write_entry(fp, 0, 0, 0, 1, 0, 0, "TRAILER!!!", NULL);
	if (err) return err;

	/* Although I couldn't find a documented reason for this, existing
	 * cpio binaries seem to pad to the next blocksize boundary. -JH */
	err = write_padding(fp, BLOCKSIZE);
	if (err) return err;
	break;
    }

    /* sync to disk */
    if (fflush(fp))
	return ENOSPC;

    return 0;
}

