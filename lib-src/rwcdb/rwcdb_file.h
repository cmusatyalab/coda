/* BLURB lgpl

                           Coda File System
                              Release 6

          Copyright (c) 2003-2016 Carnegie Mellon University
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
#include <stdint.h>

struct db_file {
    int fd;
    ino_t ino;
    uint32_t pos, len, eod;
    uint32_t cache_pos, cache_len, pending;
    void *cache;
};

/* prototypes for functions in rwcdb_file.c */
int db_file_open(struct db_file *f, const char *name, const int mode);
void db_file_close(struct db_file *f);
int db_file_seek(struct db_file *f, const uint32_t pos);
int db_file_mread(struct db_file *f, void **data, const uint32_t len,
                  const uint32_t pos);
int db_file_write(struct db_file *f, void *data, uint32_t len);
int db_file_flush(struct db_file *f);
int db_readints(struct db_file *f, uint32_t *a, uint32_t *b, uint32_t pos);

#endif /* _RWCDB_FILE_H_ */
