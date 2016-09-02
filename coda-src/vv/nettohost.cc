/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 * Routines to translate between net and host data formats.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <netinet/in.h>

#ifdef __cplusplus
}
#endif

#include <vice.h>
#include "inconsist.h"
#include "nettohost.h"

void
ntohsid(ViceStoreId *insid, ViceStoreId *outsid)
{
  outsid->HostId = ntohl(insid->HostId);
  outsid->Uniquifier = ntohl(insid->Uniquifier);
}

void
htonsid(ViceStoreId *insid, ViceStoreId *outsid)
{
  outsid->HostId = htonl(insid->HostId);
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
