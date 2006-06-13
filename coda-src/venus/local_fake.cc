/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/** \file
 *  \brief Code to handle object expansion.
 *
 * This is a generalized implementation that will (eventually) deal with
 * all the details for expanding conflicting object both for local-global and
 * server-server conflict repair.
 *
 * It might at some point even be useful for accessing backup copies of objects
 * even when they are not in conflict.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <struct.h>

#ifdef __cplusplus
}
#endif

#include "fso.h"
#include "venusvol.h"
#include "worker.h"

/* local-global conflict detection code */
int fsobj::IsToBeRepaired(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;
    
    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;
      
      CODA_ASSERT(m);
      
      /* If a cmlent is bound to an "expanded" fsobj, we need to allow
       * access to its data and can't return a conflict. This situation occurs
       * when we have a directory conflict and expand it, but the _localcache
       * replica's children show up as inconsistent due to bindings. */

      if (m->IsToBeRepaired())
	return 1;

      /* else this cmlent is not in conflict */
    }
  }
  return 0;
}

/* This function finds the uid within the first broken cmlent associated with
 * this fsobj, for use in launching ASRs. */

uid_t fsobj::WhoIsLastAuthor(void) {
  if(mle_bindings) {
    dlist_iterator next(*mle_bindings);
    dlink *d;
    
    while(( d = next() )) {
      binding *b = strbase(binding, d, bindee_handle);
      cmlent *m = (cmlent *)b->binder;
      
      CODA_ASSERT(m);
      
      /* If a cmlent is bound to an "expanded" fsobj, we need to allow
       * access to its data and can't return a conflict. This situation occurs
       * when we have a directory conflict and expand it, but the _localcache
       * replica's children show up as inconsistent due to bindings. */

      if (m->IsToBeRepaired())
	return m->uid;

      /* else this cmlent is not in conflict */
    }
  }
  return -1;
}
