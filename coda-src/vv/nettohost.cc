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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/vv/nettohost.cc,v 1.1 1996/11/22 19:09:57 braam Exp $";
#endif /*_BLURB_*/






/*
 *
 * Routines to translate between net and host data formats.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <netinet/in.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "inconsist.h"
#include "nettohost.h"

void 
htonfid(ViceFid *hfid, ViceFid *nfid) 
{
    nfid->Volume = htonl(hfid->Volume);
    nfid->Vnode = htonl(hfid->Vnode);
    nfid->Unique = htonl(hfid->Unique);
}

void 
ntohfid(ViceFid *nfid, ViceFid *hfid) 
{
    hfid->Volume = ntohl(nfid->Volume);
    hfid->Vnode = ntohl(nfid->Vnode);
    hfid->Unique = ntohl(nfid->Unique);
}

void
ntohsid(ViceStoreId *insid, ViceStoreId *outsid)
{
  outsid->Host = ntohl(insid->Host);
  outsid->Uniquifier = ntohl(insid->Uniquifier);
}

void
htonsid(ViceStoreId *insid, ViceStoreId *outsid)
{
  outsid->Host = htonl(insid->Host);
  outsid->Uniquifier = htonl(insid->Uniquifier);
}

void
ntohvv(ViceVersionVector *invv, ViceVersionVector *outvv)
{
  RPC2_Integer  *in_ptr = &( invv->Versions.Site0);
  RPC2_Integer *out_ptr = &(outvv->Versions.Site0);
  for (int i = 0; i < VSG_MEMBERS; i++)
    out_ptr[i] = ntohl(in_ptr[i]);

  ntohsid(&(invv->StoreId), &(outvv->StoreId));

  /* Also assume the flags are really a long integer */
  outvv->Flags = ntohl(invv->Flags);
}

void
htonvv(ViceVersionVector *invv, ViceVersionVector *outvv)
{
  /* Assume that the entries in a version vector array are integers */
  
  RPC2_Integer  *in_ptr = &( invv->Versions.Site0);
  RPC2_Integer *out_ptr = &(outvv->Versions.Site0);
  for (int i = 0; i < VSG_MEMBERS; i++)
    out_ptr[i] = htonl(in_ptr[i]);

  htonsid(&(invv->StoreId), &(outvv->StoreId));

  /* Also assume the flags are really a long integer */
  outvv->Flags = htonl(invv->Flags);
}
