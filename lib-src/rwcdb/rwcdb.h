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

#ifndef _RWCDB_H_
#define _RWCDB_H_

#include <sys/types.h>
#include "rwcdb_file.h"
#include "dllist.h"

struct rwcdb {
    char *file;
    struct db_file rf, wf;
    unsigned readonly : 1;
    u_int32_t hash, klen, dlen, dpos;
    struct wrentry *pending;
    char *tmpbuf;
    u_int32_t tmplen;
    u_int32_t index;
    u_int32_t hlens[256];
    struct dllist_head removed;
    struct dllist_head added[256];
};


int rwcdb_init(struct rwcdb *c, const char *file, const int mode);
int rwcdb_free(struct rwcdb *c);
int rwcdb_find(struct rwcdb *c, const char *k, const u_int32_t klen);
#define rwcdb_datalen(c) ((c)->dlen)
#define rwcdb_datapos(c) ((c)->dpos)
int rwcdb_read(struct rwcdb *c, char *d, const u_int32_t dlen,
               const u_int32_t dpos);

int rwcdb_next(struct rwcdb *c, int init);
#define rwcdb_keylen(c)  ((c)->klen)
int rwcdb_readkey(struct rwcdb *c, char *k, const u_int32_t klen,
                  const u_int32_t dpos);

int rwcdb_insert(struct rwcdb *c, const char *k, const u_int32_t klen,
                  const char *d, const u_int32_t dlen);
int rwcdb_delete(struct rwcdb *c, const char *k, const u_int32_t klen);

int rwcdb_sync(struct rwcdb *c);

#endif /* _RWCDB_H_ */
