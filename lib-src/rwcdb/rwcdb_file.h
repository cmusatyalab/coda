/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
*/

#ifndef _RWCDB_FILE_H_
#define _RWCDB_FILE_H_

#include <sys/types.h>

struct db_file {
    int fd;
    ino_t ino;
    u_int32_t pos, len, eod;
    u_int32_t cache_pos, cache_len, pending;
    void *map, *cache;
};

/* prototypes for functions in rwcdb_file.c */
int db_file_open(struct db_file *f, const char *name, const int mode);
void db_file_close(struct db_file *f);
int db_file_seek(struct db_file *f, const u_int32_t pos);
int db_file_mread(struct db_file *f, void **data, const u_int32_t len,
                  const u_int32_t pos);
int db_file_write(struct db_file *f, void *data, u_int32_t len);
int db_file_flush(struct db_file *f);
int readints(struct db_file *f, u_int32_t *a, u_int32_t *b, u_int32_t pos);
int grow_cache(struct db_file *f, u_int32_t len);
int cached(struct db_file *f, u_int32_t len, u_int32_t pos);

#endif /* _RWCDB_FILE_H_ */
