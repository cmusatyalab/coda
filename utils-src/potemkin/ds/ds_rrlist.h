#ifndef _DS_RRLIST_H_
#define _DS_RRLIST_H_

/*
   ds_rrlist.h - the resource request list data structure
*/

#include <stdio.h>

#include <odytypes.h>
#include <ds_list.h>    /* some calls return lists */

/*

   Resource request lists are used internally in the viceroy and
   wardens.  Abstractly, they contain a set of bounds, B, that have
   been requested by processes, and a current integer value, v.  Each
   bound B consists of the following:

   B.low - the low value of the bound
   B.high - the high value of the bound
   B.pid - the process id of the requesting process
   B.reqid - the identifier granted to that requesting process

   The following (public) invariants that hold for rrl's.

   forall b in B, b.low <= v <= b.high.
   forall b1, b2 in B s.t. b1!=b2, b1.pid != b2.pid
   forall time, forall b1, b2 s.t. b1!=b2, b1.reqid != b2.reqid

   (If requests on a resource come every 1/10sec, then it will take
    six years for the number of reqid's to be exhausted.)

*/

/* Individual requests */
/* 
   rrlist clients are fully responsible for allocating/deallocating
   these.  They're exposed because they are public, for all intents
   and purposes; there are no real complicated operations on them, and
   no serious invariants.  Allocation and deallocation operations are
   provided, but as macros.  Treat the reqid field as const!
   (This seemed easier than creating some opaque type unnecessarily)
*/

typedef struct ds_request_t {
    magic_t     magic;
    long        low;          /* lower bound */
    long        high;         /* upper bound */
    long        reqid;        /* const: filled only by rrlists */
    long        pid;          /* process id of requesting process */
} ds_request_t;

extern magic_t ds_request_magic;

#define DS_REQUEST_VALID(rp)   ((rp) && ((rp)->magic == ds_request_magic))

#define DS_REQUEST_ALLOCATE(X,l,h,p)   \
do {                                   \
    ALLOC((X),ds_request_t);           \
    (X)->magic = ds_request_magic;     \
    (X)->low = (l);                    \
    (X)->high = (h);                   \
    (X)->reqid = 0;                    \
    (X)->pid = (p);                    \
} while (0)

#define DS_REQUEST_DESTROY(X)                \
do {                                         \
    CODA_ASSERT(DS_REQUEST_VALID((X)));           \
    (X)->magic = 0;                          \
    (X)->low = (X)->high = (X)->reqid = 0L;  \
    (X)->pid = 0;                            \
    FREE((X));                               \
} while (0)


/* resource request lists */
typedef struct ds_rrlist_t ds_rrlist_t;    /* opaque */

/*** Return Values ***/
/* the functions can return more than one flag! */
typedef enum { 
    DS_RRLIST_SUCCESS = 0, 
    DS_RRLIST_OUTOFWINDOW = 1,
    DS_RRLIST_DUPLICATE = 2,
    DS_RRLIST_NOSUCHPID = 4,
    DS_RRLIST_NOSUCHREQ = 8
} ds_rrlist_return_t;
	       
/*** Observers ***/

extern bool          ds_rrlist_valid    (ds_rrlist_t *l);
extern long          ds_rrlist_value    (ds_rrlist_t *l);

/*** Mutators ***/

/* 
   Create a new list: specify the initial value
   of the resource.  It is assumed that no function from this
   package can be (legitimately) called before this one: it does
   any internal setup necessary.
*/
extern ds_rrlist_t *
ds_rrlist_create(long value);

/*
   Destroy a rrlist.  It must be empty.
*/
void
ds_rrlist_destroy(ds_rrlist_t *l);

/*
   Place a request.  Returns:
   DS_RRLIST_SUCCESS          if request placed without incident

   It can also return a value with one or more of the following flags
   set:

   DS_RRLIST_OUTOFWINDOW      if current value is out of the request's window
   DS_RRLIST_DUPLICATE        if there was an old request for this pid
   
   If the current value is out of the request's window, the OUT parameter
   value is set to the current value.

   If the current request supersedes an old one, the OUT parameter
   '*old_req' is set to the old outstanding request, which is removed.
*/
extern ds_rrlist_return_t
ds_rrlist_request (ds_rrlist_t *l, ds_request_t *r, long *value,
		   ds_request_t **old_req);

/*
   Cancel a request.  Returns:
   DS_RRLIST_SUCCESS          if successful
   DS_RRLIST_NOSUCHREQ        if the request isn't a valid one

   If the call is successful, req is filled with the removed request.
   Note that we don't need to (and often cannot) know what list the 
   request was granted from.
*/
extern ds_rrlist_return_t   
ds_rrlist_cancel  (long reqid, ds_request_t **req);

/*
   Purge a program's outstanding requests.  Returns
   DS_RRLIST_SUCCESS          if successful
   DS_RRLIST_NOSUCHPID        if the process has no outstanding requests

   If the call is successful, req is filled with the removed request
*/
extern ds_rrlist_return_t
ds_rrlist_purge   (ds_rrlist_t *l, int pid, ds_request_t **req);

/*
   Change the value of the resource.  Returns
   DS_RRLIST_SUCCESS          if successful

   (But, check the return value anyway.)
   The OUT parameter argument '*to_notify' is set to a (possibly NULL)
   list of ds_request_t's that no longer enclose the current value.
   It is the caller's responsibility to destroy *to_notify, which is
   a "safe" list.  See ds_list.h for more information.
*/
extern ds_rrlist_return_t
ds_rrlist_set_value (ds_rrlist_t *l, long newval, ds_list_t **to_notify);

/*
   Debugging:  print out a list.
*/
extern void
ds_rrlist_dump (ds_rrlist_t *l, FILE *f, char *name);

#endif _DS_RRLIST_H_
