#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/







/*
 *    The purpose of cliques is to subset the modified objects of a volume into independent groups.
 *    This allows us to reduce the granularity of write-back.  Advantages of finer granularity include:
 *        1. shortening of transaction duration at servers
 *        2. more opportunity for "throttling" reintegration
 *        3. lower latency on revocation events
 *        4. less interference when multiple users are mutating in a volume
 *        5. lower-cost migration
 *        6. increased locality at servers
 *        7. easier incorporation into co-volumes?
 *
 *    Cliques add another level in the synchronization hierarchy.  We now have:
 *        1. Volumes
 *        2. Cliques
 *        3. Objects (fso's)
 *
 *    Clique write-back or flushing is caused by the following events:
 *        1. token revocation (from a server) for some object in the clique
 *        2. user conflict
 *           a) clique owner's auth tokens are about to expire or be flushed
 *           b) clique's owned by different user's need to be merged
 *        3. old-age: oldest operation associated with clique exceeds threshold
 *        4. communication state change --> reintegration
 *    Note that some of these (#1 and #3, I think) may involve partial flush of the clique.
 *
 *    A clique is meant to capture dependent sets of operations (and objects) in the manner of "precedence
 *    graphs" introduced by Davidson.  A clique consists of two sets:
 *        1. The transactions that comprise a sub-graph of the precedence graph
 *        2. The file system objects that are involved in those transactions
 *    By construction, an object can belong to at most one clique ("clean" objects belong to no clique).
 *
 *    Note that when disconnection occurs, unseen operations at the server(s) can make any objects in the
 *    volume dependent upon each other.  Therefore, upon disconnection we must merge all existing cliques
 *    (in a volume) and enter all further disconnected operations/objects in this single clique.  Since we wish
 *    to avoid inter-user dependencies during reintegration, we are forced to not only limit the transaction-
 *    owners of a single clique to one user, but to limit all of the cliques in a volume to one user.  This means
 *    (among other things) that situation 2b above can never occur.
 *
 */

#include "venusvol.h"



class clique : public dlink {

    CliqueId key;
    vuid_t vuid;
    struct mutex mutex;
    struct condition sync;
    dlist *Tlist;		/* transactions belonging to this clique (in TID order) */
    dlist *Olist;		/* objects belonging to this clique (in FID order) */

  public:
    clique();
    clique(clique&);		/* not supported! */
    operator=(clique&);		/* not supported! */
    ~clique();

    void print();
    void print(FILE *);
    void print(int);
};

/*
 *
 *    Implementation of cliques, used in write-back and reintegration.
 *
 *    Every (mutating) transaction either:
 *        1. forms a new clique
 *        2. joins an existing clique
 *        3. merges two (or more) existing cliques
 *
 *    When preparing a new mutating transaction, the procedure is to derive the set of cliques corresponding
 *    to the objects involved in the transaction.  We switch on the cardinality of this set, C.
 *        - if |C| = 0, form a new clique consisting only of this transaction (and these objects)
 *        - if |C| = 1, add this transaction to the clique (and those of the objects that are not already members)
 *        - if |C| > 1, merge the cliques, and add this transaction (and any unaffiliated objects)
 *
 *    Note that for the "cancelling" operations, {StoreData, Unlink, Rmdir}, certain earlier transactions in the
 *    clique may be removed or modified.
 *
 *    Token revocation of some object in the clique (aka involuntary write-back) involves the following:
 *        - identify head transaction of clique for the requested object
 *        - compute the sequence of transactions that must be written back; this is the "prefix" which ends with
 *          the last transaction involving the requested object
 *        - write-back the prefix; note that tokens for objects other than the requested object must NOT be
 *          relinquished by the write-back except for objects that do not appear in the clique "suffix"
 *
 *    Auth token expiry/revocation of the clique owner forces write-back of the entire clique.
 *
 *    Age-driven write-back involves the prefix ending with the last "expired" transaction.  Note that, again,
 *    tokens for written-back objects can only be reliquished if the objects do not appear in the clique suffix.
 *
 *    Volume transition from hoarding to emulating state forces merge of all cliques in the volume.
 *
 */




clique::clique() {
    LOG(10, ("clique::clique:\n"));

}


clique::clique(clique& c) {
    abort();
}


clique::operator=(clique& c) {
    abort();
}


clique::~clique() {
    LOG(10, ("clique::~clique:\n"));

}


void clique::print() {
    print(stdout);
}


void clique::print(FILE *fp) {
    fflush(fp);
    print(fileno(fp));
}


void clique::print(int afd) {
    fdprint(afd, "%#08x : \n",
	     (long)this);
}
