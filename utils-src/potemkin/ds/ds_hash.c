

/*
** ds_hash.c: implementation of ds_hash_t.
*/

#include <stdlib.h>

#include <odytypes.h>

#include "ds_list.h"
#include "ds_hash.h"
#include "ds_hash.private.h"

#define DS_HASH_DEBUG 0

/* magic numbers */
const magic_t ds_hash_magic      = 214795600;
const magic_t ds_hash_iter_magic = 1144657940;

bool ds_hash_valid(ds_hash_t *t)
{
    if (DS_HASH_VALID(t))
        return TRUE;
    else
        return FALSE;
}

int ds_hash_count(ds_hash_t *t)
{
    CODA_ASSERT(DS_HASH_VALID(t));
    return t->count;
}

void *ds_hash_first(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    return (ds_list_first(chain));
}

void *ds_hash_last(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    return (ds_list_last(chain));
}

void *ds_hash_member(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    return (ds_list_member(chain, e));
}

ds_hash_t *ds_hash_create(COMPFN c, HFN h, int nbuckets, bool safe_destroy,
                          bool dups_ok)
{
    ds_hash_t *result;
    int i;

    CODA_ASSERT(h != NULL);
    CODA_ASSERT(nbuckets > 0);

    ALLOC(result, ds_hash_t);

    result->magic    = ds_hash_magic;
    result->hfn      = h;
    result->count    = 0;
    result->nbuckets = nbuckets;

    NALLOC(result->buckets, ds_list_t *, nbuckets);
    for (i = 0; i < nbuckets; i++) {
        (result->buckets)[i] = ds_list_create(c, safe_destroy, dups_ok);
    }

    return result;
}

void ds_hash_destroy(ds_hash_t *t)
{
    int i;

    CODA_ASSERT(DS_HASH_VALID(t));

    for (i = 0; i < t->nbuckets; i++) {
        ds_list_destroy((t->buckets)[i]);
    }
    t->magic    = 0;
    t->nbuckets = 0;
    t->count    = 0;
    t->hfn      = NULL;
    FREE(t->buckets);
    FREE(t);
}

void *ds_hash_insert(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;
    void *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];

    result = ds_list_insert(chain, e);
    if (result != NULL)
        t->count++;
    return result;
}

void *ds_hash_append(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;
    void *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    result = ds_list_append(chain, e);
    if (result != NULL)
        t->count++;
    return result;
}

void *ds_hash_get_first(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;
    void *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    result = ds_list_get_first(chain);
    if (result != NULL)
        t->count--;
    return result;
}

void *ds_hash_get_last(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;
    void *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    result = ds_list_get_last(chain);
    if (result != NULL)
        t->count--;
    return result;
}

void *ds_hash_remove(ds_hash_t *t, void *e)
{
    int bucket;
    ds_list_t *chain;
    void *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    bucket = (t->hfn(e)) % t->nbuckets;
    chain  = (t->buckets)[bucket];
    result = ds_list_remove(chain, e);
    if (result != NULL)
        t->count--;
    return result;
}

void ds_hash_print(ds_hash_t *t, void (*printer)(void *))
{
    int i;

    CODA_ASSERT(DS_HASH_VALID(t));
    CODA_ASSERT(printer != NULL);

    for (i = 0; i < t->nbuckets; i++) {
        if (DS_HASH_DEBUG) {
            fprintf(stderr, "**** bucket %d\n", i);
        }
        ds_list_print((t->buckets)[i], TRUE, printer);
    }
}

ds_hash_iter_t *ds_hash_iter_create(ds_hash_t *t)
{
    ds_hash_iter_t *result;

    CODA_ASSERT(DS_HASH_VALID(t));
    ALLOC(result, ds_hash_iter_t);
    result->magic     = ds_hash_iter_magic;
    result->table     = t;
    result->curbucket = 0;
    result->curiter   = ds_list_iter_create((t->buckets)[0]);

    return result;
}

void ds_hash_iter_destroy(ds_hash_iter_t *i)
{
    CODA_ASSERT(DS_HASH_ITER_VALID(i));
    if (i->curiter)
        ds_list_iter_destroy(i->curiter);
    i->magic     = 0;
    i->table     = NULL;
    i->curbucket = 0;
    i->curiter   = NULL;
    FREE(i->curiter);
}

void *ds_hash_iter_next(ds_hash_iter_t *i)
{
    void *result = NULL;
    ds_hash_t *t;

    CODA_ASSERT(DS_HASH_ITER_VALID(i));

    if (i->curiter != NULL) {
        result = ds_list_iter_next(i->curiter);

        if (result == NULL) {
            t = i->table;
            CODA_ASSERT(DS_HASH_VALID(t));

            ds_list_iter_destroy(i->curiter);
            i->curiter = NULL;
            i->curbucket++;
            while (i->curbucket < t->nbuckets) {
                if (ds_list_count((t->buckets)[i->curbucket]) > 0) {
                    i->curiter =
                        ds_list_iter_create((t->buckets)[i->curbucket]);
                    result = ds_list_iter_next(i->curiter);
                    break;
                }
                i->curbucket++;
            }
        }
    }
    return result;
}
