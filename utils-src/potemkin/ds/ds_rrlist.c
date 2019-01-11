/*
   ds_rrlist.c: implementation of ds_rrlist's.
*/

#include <odytypes.h>

#include "ds_list.h"
#include "ds_hash.h"
#include "ds_rrlist.h"
#include "ds_rrlist.private.h"

/* magic numbers */

const magic_t ds_request_magic           = 329145230;
const magic_t ds_rrlist_magic            = 219487091;
const magic_t ds_rrlist_reqtab_elt_magic = 303126476;

static long lastreqid    = 0; /* last request id assigned */
static ds_hash_t *reqtab = NULL; /* request=>rrlist mapping */

/* static functions: hash and comparison functions */

static long hash_reqtab_elt(void *anon)
{
    ds_rrlist_reqtab_elt_t *elt = anon;
    CODA_ASSERT(DS_RRLIST_REQTAB_ELT_VALID(elt));
    return elt->reqid;
}

static long comp_reqtab_elt(void *a1, void *a2)
{
    ds_rrlist_reqtab_elt_t *e1 = a1;
    ds_rrlist_reqtab_elt_t *e2 = a2;
    CODA_ASSERT(DS_RRLIST_REQTAB_ELT_VALID(e1));
    CODA_ASSERT(DS_RRLIST_REQTAB_ELT_VALID(e2));
    return (e1->reqid - e2->reqid);
}

/* 
   pids and reqids are unique in each structure by invariant,
   so they are sufficient for distinction
*/

static long hashpid(void *anon)
{
    ds_request_t *request = anon;
    CODA_ASSERT(DS_REQUEST_VALID(request));
    return request->pid;
}

static long hashreq(void *anon)
{
    ds_request_t *request = anon;
    CODA_ASSERT(DS_REQUEST_VALID(request));
    return request->reqid;
}

static long comppid(void *a1, void *a2)
{
    ds_request_t *r1 = a1;
    ds_request_t *r2 = a2;

    CODA_ASSERT(DS_REQUEST_VALID(r1));
    CODA_ASSERT(DS_REQUEST_VALID(r2));

    return (r1->pid - r2->pid);
}

static long compreq(void *a1, void *a2)
{
    ds_request_t *r1 = a1;
    ds_request_t *r2 = a2;

    CODA_ASSERT(DS_REQUEST_VALID(r1));
    CODA_ASSERT(DS_REQUEST_VALID(r2));

    return (r1->reqid - r2->reqid);
}

/* 
   lists can contain many elements with the same low/high values.  As
   long as they are ordered correctly with respect to those values, we
   can use the pointers themselves to distinguish equal bounds values.
   This is necessary since otherwise equality tests for ds_list_remove
   calls would be too weak.
*/

static long complow(void *a1, void *a2)
{
    ds_request_t *r1 = a1;
    ds_request_t *r2 = a2;
    long result;

    CODA_ASSERT(DS_REQUEST_VALID(r1));
    CODA_ASSERT(DS_REQUEST_VALID(r2));

    result = r1->low - r2->low;
    if (!result)
        result = (r1->reqid - r2->reqid);
    return result;
}

static long comphigh(void *a1, void *a2)
{
    ds_request_t *r1 = a1;
    ds_request_t *r2 = a2;
    long result;

    CODA_ASSERT(DS_REQUEST_VALID(r1));
    CODA_ASSERT(DS_REQUEST_VALID(r2));

    result = r1->high - r2->high;
    if (!result)
        result = (r1->reqid - r2->reqid);
    return result;
}

/* Observers */

bool ds_rrlist_valid(ds_rrlist_t *l)
{
    if (DS_RRLIST_VALID(l))
        return TRUE;
    else
        return FALSE;
}

long ds_rrlist_value(ds_rrlist_t *l)
{
    CODA_ASSERT(DS_RRLIST_VALID(l));
    return l->value;
}

/* Mutators */

ds_rrlist_t *ds_rrlist_create(long value)
{
    ds_rrlist_t *result;

    /* Is the universal hashtab setup yet? */
    /* Check here b/c it is cheap. */
    if (reqtab == NULL) {
        /* not yet, do it. */
        reqtab = ds_hash_create(comp_reqtab_elt, hash_reqtab_elt,
                                DS_RRLIST_HASHSIZE, TRUE, FALSE);
    }

    ALLOC(result, ds_rrlist_t);
    result->magic = ds_rrlist_magic;
    result->value = value;
    result->pidhash =
        ds_hash_create(comppid, hashpid, DS_RRLIST_HASHSIZE, TRUE, FALSE);
    result->reqhash =
        ds_hash_create(compreq, hashreq, DS_RRLIST_HASHSIZE, TRUE, FALSE);
    result->less    = ds_list_create(complow, TRUE, FALSE);
    result->greater = ds_list_create(comphigh, TRUE, FALSE);

    return result;
}

void ds_rrlist_destroy(ds_rrlist_t *l)
{
    CODA_ASSERT(DS_RRLIST_VALID(l));
    l->magic = 0;
    l->value = 0;
    ds_hash_destroy(l->pidhash);
    ds_hash_destroy(l->reqhash);
    ds_list_destroy(l->less);
    ds_list_destroy(l->greater);
    FREE(l);
}

ds_rrlist_return_t ds_rrlist_request(ds_rrlist_t *l, ds_request_t *r,
                                     long *value, ds_request_t **old_req)
{
    ds_rrlist_return_t result = DS_RRLIST_SUCCESS;
    ds_rrlist_reqtab_elt_t *rte;
    ds_rrlist_reqtab_elt_t dummy;

    CODA_ASSERT(DS_RRLIST_VALID(l));
    CODA_ASSERT(DS_REQUEST_VALID(r));
    CODA_ASSERT(value);
    CODA_ASSERT(old_req);

    r->reqid = 0;

    /* do we have a current request for this pid/resource pair? */
    *old_req = ds_hash_member(l->pidhash, r);
    if (*old_req != NULL) {
        result |= DS_RRLIST_DUPLICATE;
        /* remove from reqtab */
        dummy.magic = ds_rrlist_reqtab_elt_magic;
        dummy.reqid = (*old_req)->reqid;
        CODA_ASSERT((rte = ds_hash_remove(reqtab, &dummy)) != NULL);
        DS_RRLIST_REQTAB_ELT_DESTROY(rte);
        /* remove from rrlist */
        CODA_ASSERT(ds_hash_remove(l->pidhash, *old_req) != NULL);
        CODA_ASSERT(ds_hash_remove(l->reqhash, *old_req) != NULL);
        CODA_ASSERT(ds_list_remove(l->less, *old_req) != NULL);
        CODA_ASSERT(ds_list_remove(l->greater, *old_req) != NULL);
    }

    /* Is the request out-of-bounds already? */
    if ((r->low > l->value) || (r->high < l->value)) {
        result |= DS_RRLIST_OUTOFWINDOW;
        *value = l->value;
    } else {
        lastreqid++;
        r->reqid = lastreqid;
        DS_RRLIST_REQTAB_ELT_CREATE(rte, lastreqid, l);
        CODA_ASSERT(ds_hash_insert(reqtab, rte) != NULL);
        /* it is a valid request, insert it. */
        CODA_ASSERT(ds_hash_insert(l->pidhash, r) != NULL);
        CODA_ASSERT(ds_hash_insert(l->reqhash, r) != NULL);
        CODA_ASSERT(ds_list_insert(l->less, r) != NULL);
        CODA_ASSERT(ds_list_insert(l->greater, r) != NULL);
    }
    return result;
}

ds_rrlist_return_t ds_rrlist_cancel(long reqid, ds_request_t **req)
{
    ds_rrlist_return_t result = DS_RRLIST_SUCCESS;
    ds_request_t req_dummy;
    ds_rrlist_t *l;
    ds_rrlist_reqtab_elt_t *rte;
    ds_rrlist_reqtab_elt_t rte_dummy;

    CODA_ASSERT(req);

    /* First, we need to get the list this request is on */
    rte_dummy.magic = ds_rrlist_reqtab_elt_magic;
    rte_dummy.reqid = reqid;
    rte             = ds_hash_member(reqtab, &rte_dummy);

    if (rte == NULL) { /* didn't find the list */
        result |= DS_RRLIST_NOSUCHREQ;
        *req = NULL;
    } else {
        /* we have the list, continue */
        l = rte->list;
        CODA_ASSERT(DS_RRLIST_VALID(l));
        /* See if we can find the item to remove. */
        req_dummy.magic = ds_request_magic;
        req_dummy.reqid = reqid;
        *req            = ds_hash_member(l->reqhash, &req_dummy);
        if (*req) {
            /* found it */
            CODA_ASSERT(DS_REQUEST_VALID(*req));
            /* Pull from the global hashtab */
            CODA_ASSERT(ds_hash_remove(reqtab, rte) == rte);
            DS_RRLIST_REQTAB_ELT_DESTROY(rte);
            /* Pull from the resource request list in question */
            CODA_ASSERT(ds_hash_remove(l->pidhash, *req) != NULL);
            CODA_ASSERT(ds_hash_remove(l->reqhash, *req) != NULL);
            CODA_ASSERT(ds_list_remove(l->less, *req) != NULL);
            CODA_ASSERT(ds_list_remove(l->greater, *req) != NULL);
        } else {
            result |= DS_RRLIST_NOSUCHREQ;
        }
    }
    return result;
}

ds_rrlist_return_t ds_rrlist_purge(ds_rrlist_t *l, int pid, ds_request_t **req)
{
    ds_rrlist_return_t result = DS_RRLIST_SUCCESS;
    ds_request_t req_dummy;
    ds_rrlist_reqtab_elt_t rte_dummy;
    ds_rrlist_reqtab_elt_t *rte;

    CODA_ASSERT(DS_RRLIST_VALID(l));
    CODA_ASSERT(req);

    /* See if we can find the item to remove */
    req_dummy.magic = ds_request_magic;
    req_dummy.pid   = pid;
    *req            = ds_hash_member(l->pidhash, &req_dummy);
    if (*req) {
        /* found it */
        CODA_ASSERT(DS_REQUEST_VALID(*req));
        /* remove from reqtab */
        rte_dummy.magic = ds_rrlist_reqtab_elt_magic;
        rte_dummy.reqid = (*req)->reqid;
        CODA_ASSERT((rte = ds_hash_remove(reqtab, &rte_dummy)) != NULL);
        DS_RRLIST_REQTAB_ELT_DESTROY(rte);
        /* remove from rrlist */
        CODA_ASSERT(ds_hash_remove(l->pidhash, *req) == *req);
        CODA_ASSERT(ds_hash_remove(l->reqhash, *req) == *req);
        CODA_ASSERT(ds_list_remove(l->less, *req) == *req);
        CODA_ASSERT(ds_list_remove(l->greater, *req) == *req);
    } else {
        result |= DS_RRLIST_NOSUCHPID;
    }
    return result;
}

ds_rrlist_return_t ds_rrlist_set_value(ds_rrlist_t *l, long newval,
                                       ds_list_t **to_notify)
{
    ds_rrlist_return_t result = DS_RRLIST_SUCCESS;
    ds_request_t *req         = NULL;
    ds_rrlist_reqtab_elt_t dummy;
    ds_rrlist_reqtab_elt_t *rte;

    CODA_ASSERT(DS_RRLIST_VALID(l));
    CODA_ASSERT(to_notify);

    /* zero the list pointer */
    *to_notify = NULL;

    /* 
       check quickly if we have to do anything, and get out if not.
       (the common case)
    */
    if (newval > l->value) {
        req = (ds_request_t *)ds_list_first(l->greater);
        if ((req == NULL) || (newval <= req->high)) {
            l->value = newval;
            return result;
        }
    } else {
        req = (ds_request_t *)ds_list_last(l->less);
        if ((req == NULL) || (newval >= req->low)) {
            l->value = newval;
            return result;
        }
    }

    /* 
       there is at least one bound that has been exceeded: req
       currently points to it.  l->value has not yet been set,
       and the to_notify list must be constructed
    */

    *to_notify = ds_list_create(NULL, FALSE, FALSE);

    if (newval > l->value) {
        /* check upper bounds */
        while (req != NULL) {
            CODA_ASSERT(DS_REQUEST_VALID(req));
            if (req->high >= newval) {
                break;
            } else {
                /* remove from reqtab */
                dummy.magic = ds_rrlist_reqtab_elt_magic;
                dummy.reqid = req->reqid;
                CODA_ASSERT((rte = ds_hash_remove(reqtab, &dummy)) != NULL);
                DS_RRLIST_REQTAB_ELT_DESTROY(rte);
                /* remove from the rrlist */
                CODA_ASSERT(ds_list_get_first(l->greater) == req);
                CODA_ASSERT(ds_list_remove(l->less, req) == req);
                CODA_ASSERT(ds_hash_remove(l->pidhash, req) == req);
                CODA_ASSERT(ds_hash_remove(l->reqhash, req) == req);
                /* insert into result list */
                CODA_ASSERT(ds_list_insert(*to_notify, req) == req);
                /* step to next pointer in list. */
                req = (ds_request_t *)ds_list_first(l->greater);
            }
        }
    } else {
        /* check lower bounds */
        while (req != NULL) {
            CODA_ASSERT(DS_REQUEST_VALID(req));
            if (req->low <= newval) {
                break;
            } else {
                /* remove from reqtab */
                dummy.magic = ds_rrlist_reqtab_elt_magic;
                dummy.reqid = req->reqid;
                CODA_ASSERT((rte = ds_hash_remove(reqtab, &dummy)) != NULL);
                DS_RRLIST_REQTAB_ELT_DESTROY(rte);
                /* remove from the rrlist */
                CODA_ASSERT(ds_list_get_last(l->less) == req);
                CODA_ASSERT(ds_list_remove(l->greater, req) == req);
                CODA_ASSERT(ds_hash_remove(l->pidhash, req) == req);
                CODA_ASSERT(ds_hash_remove(l->reqhash, req) == req);
                /* insert into result list */
                CODA_ASSERT(ds_list_insert(*to_notify, req) == req);
                /* step to next pointer in list. */
                req = (ds_request_t *)ds_list_last(l->less);
            }
        }
    }
    l->value = newval;
    return result;
}

static void ds_request_print(ds_request_t *req, FILE *f)
{
    CODA_ASSERT(DS_REQUEST_VALID(req));
    fprintf(f, "pid %ld\treqid %ld\tlow %ld\thigh %ld\n", req->pid, req->reqid,
            req->low, req->high);
}

void ds_rrlist_dump(ds_rrlist_t *l, FILE *f, char *name)
{
    ds_hash_iter_t *hi;
    ds_list_iter_t *li;
    ds_request_t *req;

    CODA_ASSERT(DS_RRLIST_VALID(l));
    CODA_ASSERT(f);
    CODA_ASSERT(name);

    fprintf(f, "Printing resource request list %s\n", name);
    fprintf(f, "========================================================\n");
    fprintf(f, "--------value = %ld\n", l->value);
    fprintf(f, "--------last requid assigned = %ld\n", lastreqid);
    fprintf(f, "--------pidhash table\n");

    hi = ds_hash_iter_create(l->pidhash);
    while ((req = (ds_request_t *)ds_hash_iter_next(hi)) != NULL) {
        ds_request_print(req, f);
    }
    ds_hash_iter_destroy(hi);

    fprintf(f, "--------reqhash table\n");

    hi = ds_hash_iter_create(l->reqhash);
    while ((req = (ds_request_t *)ds_hash_iter_next(hi)) != NULL) {
        ds_request_print(req, f);
    }
    ds_hash_iter_destroy(hi);

    fprintf(f, "--------lower bound list\n");

    li = ds_list_iter_create(l->less);
    while ((req = (ds_request_t *)ds_list_iter_next(li)) != NULL) {
        ds_request_print(req, f);
    }
    ds_list_iter_destroy(li);

    fprintf(f, "--------upper bound list\n");

    li = ds_list_iter_create(l->greater);
    while ((req = (ds_request_t *)ds_list_iter_next(li)) != NULL) {
        ds_request_print(req, f);
    }
    ds_list_iter_destroy(li);

    fprintf(f, "\n\n");
}
