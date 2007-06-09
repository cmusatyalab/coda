/* BLURB lgpl
			   Coda File System
			      Release 6

	  Copyright (c) 2003 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define __USE_UNIX98 /* to get pread */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rwcdb_pack.h"
#include "rwcdb.h"

/*=====================================================================*/
/* scratch buffer */

int grow_cache(struct db_file *f, u_int32_t len)
{
    f->cache_pos = f->len;
    if (f->cache_len >= len)
	return 0;

    if (f->cache) free(f->cache);
    f->cache = (char *)malloc(len);
    if (!f->cache) {
	f->cache = NULL;
	return -1;
    }
    f->cache_len = len;
    return 0;
}

int cached(struct db_file *f, u_int32_t len, u_int32_t pos)
{
    return (pos >= f->cache_pos && pos + len <= f->cache_pos + f->cache_len);
}

/*=====================================================================*/
/* fileio */

int db_file_seek(struct db_file *f, const u_int32_t pos)
{
    if (lseek(f->fd, pos, SEEK_SET) != pos) return -1;
    f->pos = pos;
    return 0;
}

int db_file_mread(struct db_file *f, void **data, const u_int32_t len,
		  const u_int32_t pos)
{
    ssize_t n;

    if (pos + len > f->len)
	return -1;

    if (!cached(f, len, pos)) {
	if (grow_cache(f, len))
	    return -1;

#ifdef HAVE_PREAD
	n = pread(f->fd, f->cache, len, pos);
#else
	if (pos != f->pos && lseek(f->fd, pos, SEEK_SET) != pos)
	    return -1;

	n = read(f->fd, f->cache, len);
#endif
	if (n >= 0)
	    f->pos = pos + n;
	if (n != len) return -1;

	f->cache_pos = pos;
	f->cache_len = len;
    }

    *data = (char *)f->cache + (pos - f->cache_pos);
    return 0;
}

#define PAGESIZE (4 * 1024)

int db_file_write(struct db_file *f, void *data, u_int32_t len)
{
    ssize_t n;
    u_int32_t blob;

    if (!len) return 0;

    if (grow_cache(f, PAGESIZE))
	return -1;

    /* first fill the rest of the page */
    blob = len < (PAGESIZE - f->pending) ? len : PAGESIZE - f->pending;
    memcpy((char *)f->cache + f->pending, data, blob);
    f->pending += blob;
    data = (char *)data + blob;
    len -= blob;
    f->pos += blob;

    /* flush full pages */
    if (f->pending == PAGESIZE) {
	n = write(f->fd, f->cache, PAGESIZE);
	if (n == -1) return -1;
	f->pending = 0;
    }

    /* flush rest of the data, when it is a lot */
    blob = len & ~(PAGESIZE - 1);
    if (blob) {
	n = write(f->fd, data, blob);
	if (n == -1) return -1;
	data = (char *)data + blob;
	len -= blob;
	f->pos += blob;
    }

    /* start filling the next page with the leftovers */
    if (len) {
	memcpy(f->cache, data, len);
	f->pending += len;
	f->pos += len;
    }

    if (f->pos > f->len)
	f->len = f->pos;

    return 0;
}

int db_file_flush(struct db_file *f)
{
    ssize_t n;
    if (f->pending) {
	n = write(f->fd, f->cache, f->pending);
	if (n == -1) return -1;
	f->pending = 0;
    }
    return 0;
}

int readints(struct db_file *f, u_int32_t *a, u_int32_t *b, u_int32_t pos)
{
    void *buf;

    if (db_file_mread(f, &buf, 8, pos))
	return -1;

    unpackints(buf, a, b);
    return 0;
}

int db_file_open(struct db_file *f, const char *name, const int mode)
{
    struct stat sb;
    u_int32_t dummy;

    f->pos = 0;
    f->eod = 2048;
    f->cache = NULL;
    f->cache_len = f->pending = 0;

    f->fd = open(name, mode, 0600);
    if (f->fd == -1) return -1;

    if (fstat(f->fd, &sb)) {
	close(f->fd);
	return -1;
    }
    f->ino = sb.st_ino;
    f->len = f->cache_pos = sb.st_size;

    if (f->len)
	(void)readints(f, &f->eod, &dummy, 0);

    return 0;
}

void db_file_close(struct db_file *f)
{
    if (f->cache) {
	free(f->cache);
	f->cache_len = 0;
    }

    if (f->fd != -1) {
	close(f->fd);
	f->fd = -1;
    }
}

