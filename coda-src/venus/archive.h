/* BLURB gpl

			    Coda File System
				Release 6

		Copyright (c) 2008 Carnegie Mellon University
		    Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

			Additional copyrights
			    none currently
#*/

#ifndef _ARCHIVE_H_
#define _ARCHIVE_H_

#include <sys/types.h>
#include <stdio.h>

#define TAR_TAR 0 /* only supports 100 character path names */
#define TAR_USTAR 1 /* up to 255 character path, only 100 character component */
#define CPIO_ODC 2 /* short inode/uid numbers */
#define CPIO_NEWC 3 /* 32-bit file size */

extern int archive_type;

int archive_write_entry(FILE *fp, ino_t inode, mode_t mode, uid_t uid,
                        nlink_t nlink, time_t mtime, size_t filesize,
                        const char *name, const char *linkname);
int archive_write_data(FILE *fp, const char *container);
int archive_write_trailer(FILE *fp);

#endif /* _ARCHIVE_H_ */
