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
#include <sys/param.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include "rwcdb_pack.h"
#include "rwcdb.h"

/*=====================================================================*/
/* some file related ops */

static int owrite(struct rwcdb *c)
{
    char newname[MAXPATHLEN];

    if (c->readonly) return -1;
    if (c->wf.fd != -1) return 0;

    strcpy(newname, c->file);
    strcat(newname, ".tmp");

    if (db_file_open(&c->wf, newname, O_CREAT | O_EXCL | O_WRONLY)) {
        fprintf(stderr, "RWCDB error: open %s failed, multiple writers?\n",
                newname);
        return -1;
    }
    return 0;
}

static void discard_pending_updates(struct rwcdb *c);

static void checkdb(struct rwcdb *c)
{
    struct stat sb;

    /* reopen the database, but only if it has been modified */
    if (stat(c->file, &sb) != 0 || sb.st_ino == c->rf.ino)
	return;

    db_file_close(&c->rf);
    if (db_file_open(&c->rf, c->file, O_RDONLY)) {
	/* Ouch, we just hosed ourselves big time */
	abort();
    }
    c->index = 2048;

    discard_pending_updates(c);
}

/*=====================================================================*/
/* wrentries are linked together to store pending updates in memory */

struct wrentry {
    struct dllist_head list;
    uint32_t hash;
    uint32_t pos;
    uint32_t klen;
    uint32_t dlen;
};

/* behind the struct wrentry is a variable length area that contains key/data */
#define wbuf(w) (((char *)w) + sizeof(struct wrentry) - 8)
#define wkey(w) (wbuf(w) + 8)
#define wdata(w) (wbuf(w) + 8 + (w)->klen)

static struct wrentry *alloc_wrentry(const uint32_t klen, const uint32_t dlen,
                                     const uint32_t hash)
{
    struct wrentry *w;

    w = (struct wrentry *)malloc(sizeof(struct wrentry) + 8 + klen + dlen);
    if (!w) return NULL;

    w->hash = hash;
    w->klen = klen;
    w->dlen = dlen;
    return w;
}

static void free_wrentry(struct wrentry *w)
{
    list_del(&w->list);
    free(w);
}


static struct wrentry *ispending(struct rwcdb *c, const char *k,
                                 const uint32_t klen, const uint32_t hash,
                                 uint32_t *index)
{
    struct dllist_head *p;
    struct wrentry *w;
    uint32_t i, idx = 0;

    list_for_each(p, c->removed)
    {
        w = list_entry(p, struct wrentry, list); 
        if (w->hash == hash && klen == w->klen &&
            memcmp(wkey(w), k, klen) == 0) {
            return w;
        }
    }

    if (!c->hlens[hash & 0xff]) return NULL;

    if (index) {
        for (i = 0; i < (hash & 0xff); i++)
            idx += c->hlens[i];
    }

    list_for_each(p, c->added[hash & 0xff])
    {
        w = list_entry(p, struct wrentry, list); 
        if (w->hash == hash && klen == w->klen &&
            memcmp(wkey(w), k, klen) == 0)
        {
            if (index) *index = idx;
            return w;
        }
        idx++;
    }
    return NULL;
}

static struct wrentry *fromhash(struct rwcdb *c, uint32_t index)
{
    struct dllist_head *p;
    uint32_t i;

    for (i = 0; i < 256; i++) {
        if (index >= c->hlens[i]) {
            index -= c->hlens[i];
            continue;
        }
        list_for_each(p, c->added[i])
        {
            if (index--) continue;
            return list_entry(p, struct wrentry, list);
        }
    }
    return NULL;
}

static void discard_pending_updates(struct rwcdb *c)
{
    struct dllist_head *p;
    struct wrentry *w;
    uint32_t i;

    /* discard left-over in-memory modifications */
    for (i = 0; i < 256; i++) {
        for (p = c->added[i].next; p != &c->added[i];) {
            w = list_entry(p, struct wrentry, list);
            p = p->next;
            free_wrentry(w);
        }
    }
    for (p = c->removed.next; p != &c->removed;) {
        w = list_entry(p, struct wrentry, list);
        p = p->next;
        free_wrentry(w);
    }
    memset(c->hlens, 0, 256 * sizeof(uint32_t));
}

/*=====================================================================*/
/* The cdb hash function is "h = ((h << 5) + h) ^ c", with a starting
 * hash of 5381 */

static uint32_t cdb_hash(const char *k, const uint32_t klen)
{
    uint32_t i, hash = 5381;

    for (i = 0; i < klen; i++)
        hash = ((hash << 5) + hash) ^ ((unsigned char)k[i]);

    return hash;
}

/*=====================================================================*/
/* main functions */

int rwcdb_init(struct rwcdb *c, const char *file, const int mode)
{
    uint32_t i, n;
    
    n = strlen(file) + 1;
    if (n > MAXPATHLEN - 5) return -1;

    memset(c, 0, sizeof(struct rwcdb));

    c->file = strdup(file);
    if (!c->file) return -1;

    c->removed.next = c->removed.prev = &c->removed;
    for (i = 0; i < 256; i++)
        c->added[i].next = c->added[i].prev = &c->added[i];

    c->readonly = ((mode & O_ACCMODE) == O_RDONLY);
    c->rf.fd = c->wf.fd = -1;
    if (db_file_open(&c->rf, file, O_RDONLY) == 0)
        return 0;
    
    if (c->rf.fd != -1)
	goto err_out;

    /* try to create a new database */
    if (owrite(c) != -1 && rwcdb_sync(c) != -1)
        return 0;

    db_file_close(&c->wf);

err_out:
    if (c->file) {
	free(c->file);
	c->file = NULL;
    }
    return -1;
}

int rwcdb_free(struct rwcdb *c)
{
    if (rwcdb_sync(c) == -1)
        return -1;

    /* just in case someone was modifying a readonly database */
    discard_pending_updates(c);

    db_file_close(&c->rf);

    if (c->file) {
	free(c->file);
	c->file = NULL;
    }
    return 1;
}

int rwcdb_find(struct rwcdb *c, const char *k, const uint32_t klen)
{
    uint32_t loop, hash, hash2, hpos, hlen, pos, keylen, cur_pos, dlen;
    struct wrentry *w;
    void *buf;

    /* A record is located as follows. Compute the hash value of the key in
     * the record. The hash value modulo 256 is the number of a hash table.
     * The hash value divided by 256, modulo the length of that table, is a
     * slot number. Probe that slot, the next higher slot, and so on, until
     * you find the record or run into an empty slot. */
    hash = cdb_hash(k, klen);

    /* first check whether there are pending modifications for this key */
    w = ispending(c, k, klen, hash, &loop);
    if (w) {
        if (!w->dlen) return 0;
        c->klen = w->klen;
        c->dlen = w->dlen;
        c->dpos = c->rf.eod + loop;
        return 1;
    }

    /* Each of the 256 initial pointers states a position and a length. The
     * position is the starting position of the hash table. The length is the
     * number of slots in the hash table. */
    cur_pos = (hash & 0xff) << 3;
    if (readints(&c->rf, &hpos, &hlen, cur_pos))
        return -1;

    /* micro-optimization? */
    if (hlen == 0) return 0;

    cur_pos = hpos + (((hash >> 8) % hlen) << 3);

    for (loop = 0; loop < hlen; loop++) {
        /* Each hash table slot states a hash value and a byte position. If
         * the byte position is 0, the slot is empty. Otherwise, the slot
         * points to a record whose key has that hash value. */
        if (readints(&c->rf, &hash2, &pos, cur_pos))
            return -1;

        if (pos == 0) return 0;

        cur_pos += 8;
        if (cur_pos == hpos + (hlen << 3))
            cur_pos = hpos;

        if (hash != hash2) continue;

        /* Records are stored sequentially, without special alignment. A
         * record states a key length, a data length, the key, and the data. */
        if (readints(&c->rf, &keylen, &dlen, pos))
            return -1;

        if (klen != keylen) continue;

        if (db_file_mread(&c->rf, &buf, klen + dlen, c->rf.pos))
            return -1;

        if (memcmp(k, buf, klen) == 0) {
            c->klen = klen;
            c->dlen = dlen;
            c->dpos = c->rf.pos - dlen;
            return 1;
        }
    }
    return 0;
}

int rwcdb_insert(struct rwcdb *c, const char *k, const uint32_t klen,
                  const char *d, const uint32_t dlen)
{
    struct wrentry *w, *old;
    uint32_t hash, slot;
    static uint32_t warned = 0;

    if (c->readonly) {
	if (!warned++)
	    fprintf(stderr, "RWCDB warning: modifying read-only database\n");
    }
    else if (owrite(c) == -1)
	return -1;

    hash = cdb_hash(k, klen);
    w = alloc_wrentry(klen, dlen, hash);
    if (!w) return -1;

    memcpy(wkey(w), k, klen);
    if (dlen) memcpy(wdata(w), d, dlen);

    old = ispending(c, k, klen, hash, NULL);

    slot = hash & 0xff;
    if (old) {
        if (old->dlen)
            c->hlens[slot]--;
        free_wrentry(old);
    }

    if (dlen) {
        list_add(&w->list, c->added[slot].prev);
        c->hlens[slot]++;
    } else
        list_add(&w->list, c->removed.prev);

    return 1;
}

int rwcdb_delete(struct rwcdb *c, const char *k, const uint32_t klen)
{
    if (rwcdb_find(c, k, klen) != 1) return 0;
    return rwcdb_insert(c, k, klen, NULL, 0);
}

int rwcdb_next(struct rwcdb *c, int init)
{
    uint32_t klen, dlen, hash;
    void *buf;

    if (init) c->index = 2048;

again:
    /* reading from disk? */
    if (c->index < c->rf.eod) {
        if (readints(&c->rf, &klen, &dlen, c->index))
            goto read_failed;

        if (db_file_mread(&c->rf, &buf, klen + dlen, c->rf.pos))
            goto read_failed;

        hash = cdb_hash(buf, klen);
        c->pending = ispending(c, buf, klen, hash, NULL);

        c->index = c->rf.pos;

        /* already on the write queue? skip entry */
        if (c->pending) goto again;

        c->hash = hash;
        c->klen = klen;
        c->dlen = dlen;
        c->dpos = c->rf.pos - dlen;
        return 1;
    }

    /* sweep through the pending write hashes */
    c->pending = fromhash(c, c->index - c->rf.eod);
    if (!c->pending) return 0;

    c->klen = c->pending->klen;
    c->dlen = c->pending->dlen;
    c->dpos = c->index;

    c->index++;
    return 1;

read_failed:
    fprintf(stderr, "RWCDB rwcdb_next: read failed, %s corrupt?\n", c->file);
    return -1;
}

int rwcdb_read(struct rwcdb *c, char *d, const uint32_t dlen,
               const uint32_t dpos)
{
    struct wrentry *w;
    void *buf;

    /* still reading on-disk information? */
    if (dpos < c->rf.eod) {
        if (db_file_mread(&c->rf, &buf, dlen, dpos))
            return -1;
    } else {
        /* sweep through the pending write hashes */
        w = fromhash(c, dpos - c->rf.eod);
        if (!w || dlen > w->dlen) return -1;
        buf = wdata(w);
    }
    memcpy(d, buf, dlen);

    return 0;
}

int rwcdb_readkey(struct rwcdb *c, char *k, const uint32_t klen,
                  const uint32_t dpos)
{
    struct wrentry *w;
    void *buf;

    if (dpos < c->rf.eod) {
        if (db_file_mread(&c->rf, &buf, klen, dpos - klen))
            return -1;
    } else {
        /* sweep through the pending write hashes */
        w = fromhash(c, dpos - c->rf.eod);
        if (!w || klen != w->klen) return -1;
        buf = wkey(w);
    }
    memcpy(k, buf, klen);

    return 0;
}

/*=====================================================================*/
/* writing new databases */

static int dump_records(struct rwcdb *c, struct dllist_head *old)
{
    int ret, rewind = 1;
    struct wrentry *w;
    char ints[8];
    void *buf;

    while(1) {
        ret = rwcdb_next(c, rewind);
        if (ret != 1) return ret;
        rewind = 0;

        if (!c->pending) {
            w = alloc_wrentry(0, 0, c->hash);
            if (!w) {
                fprintf(stderr, "RWCDB dump_records: allocation failed?\n");
                return -1;
            }
            list_add(&w->list, old->prev);
            if (db_file_mread(&c->rf, &buf, c->klen+c->dlen, c->dpos - c->klen))
                return -1;
        }
        else {
            w = c->pending;
            buf = wkey(w);
        }

        /* store position of this record */
        w->pos = c->wf.pos;

        packints(ints, c->klen, c->dlen);

        if (db_file_write(&c->wf, ints, 8) ||
            db_file_write(&c->wf, buf, c->klen+c->dlen)) {
            fprintf(stderr, "RWCDB dump_records: write failed, out of diskspace?\n");
            return -1;
        }
    }
    return 0;
}

static int write_hashchains(struct rwcdb *c)
{
    uint32_t i, slot, hoffs[256], len, maxlen, totallen;
    struct wrentry *w;
    struct dllist_head *p;
    struct rwcdb_tuple *h;

    maxlen = 256; totallen = 0;
    for (i = 0; i < 256; i++) {
        len = c->hlens[i] * 2;
        if (len > maxlen) maxlen = len;
        totallen += len;
    }

    h = (struct rwcdb_tuple *)malloc(maxlen * sizeof(struct rwcdb_tuple));
    if (!h) {
        fprintf(stderr, "RWCDB write_hashchains: allocation failed\n");
        return -1;
    }

    for (i = 0; i < 256; i++) {
        len = c->hlens[i] * 2;
        hoffs[i] = c->wf.pos;
        if (len == 0) continue;

        memset(h, 0, len * sizeof(struct rwcdb_tuple));

        list_for_each(p, c->added[i]) {
            w = list_entry(p, struct wrentry, list);
            slot = (w->hash >> 8) % len;
            while (h[slot].a) {
                if (++slot == len)
                    slot = 0;
            }
            packints((char *)&h[slot], w->hash, w->pos);
        }

        if (db_file_write(&c->wf, h, len * sizeof(struct rwcdb_tuple)))
            goto write_failed;
    }

    /* dump the header */
    for (i = 0; i < 256; i++)
        packints((char *)(&h[i]), hoffs[i], c->hlens[i] * 2);
    if (db_file_flush(&c->wf) ||
        db_file_seek(&c->wf, 0) ||
        db_file_write(&c->wf, h, 256 * sizeof(struct rwcdb_tuple)) ||
        db_file_flush(&c->wf))
        goto write_failed;

    free(h);
    return 0;

write_failed:
    fprintf(stderr, "RWCDB rwcdb_write_hashchains: write failed, out of diskspace?\n");
    free(h);
    return -1;
}

int rwcdb_sync(struct rwcdb *c)
{
    uint32_t i;
    struct dllist_head *p, rewrites;
    struct wrentry *w;
    char newname[MAXPATHLEN];

    if (c->wf.fd == -1) {
        /* see if the on-disk database was updated */
        checkdb(c);
        return 1;
    }

    rewrites.prev = rewrites.next = &rewrites;

    if (db_file_seek(&c->wf, 2048))
        return -1;

    if (dump_records(c, &rewrites))
        goto recover;

    /* remember EOD */
    c->wf.eod = c->wf.pos;

    /* move rewritten entries into the hashlists */
    for (p = rewrites.next; p != &rewrites;) {
        w = list_entry(p, struct wrentry, list);
        p = p->next;
        list_del(&w->list);
        list_add(&w->list, &c->added[w->hash & 0xff]);
        c->hlens[w->hash & 0xff]++;
    }

    /* dump hash chains and header */
    if (write_hashchains(c))
        goto recover;

    /* force dirty buffers to disk */
    fsync(c->wf.fd);

    /* replace old db file */
    strcpy(newname, c->file);
    strcat(newname, ".tmp");
    if (rename(newname, c->file) == -1) {
        fprintf(stderr, "RWCDB rwcdb_sync: rename failed\n");
        goto recover;
    }

    db_file_close(&c->wf);
    checkdb(c);

    return 1;

recover: /* try to get back to the same state as before the failure */
    /* remove rewritten entries from the hashlists */
    for (i = 0; i < 256; i++) {
        for (p = c->added[i].next; p != &c->added[i];) {
            w = list_entry(p, struct wrentry, list);
            p = p->next;
            if (!w->klen && !w->dlen) {
                free_wrentry(w);
                c->hlens[i]--;
            }
        }
    }
    for (p = rewrites.next; p != &rewrites;) {
        w = list_entry(p, struct wrentry, list);
        p = p->next;
        free_wrentry(w);
    }
    return -1;
}

