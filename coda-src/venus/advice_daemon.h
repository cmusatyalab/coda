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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/venus/advice_daemon.h,v 4.1 97/01/08 21:51:18 rvb Exp $";
#endif /*_BLURB_*/




/*
 *
 * Specification of the Venus Advice Monitor server.
 *
 */

#ifndef _VENUS_ADVICEDAEMON_H_
#define _VENUS_ADVICEDAEMON_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <admon.h>
#include "vproc.h"

extern int AdviceEnabled;
extern int ASRallowed;

const int DFLT_MAXAMSERVERS = 1;
const int UNSET_MAXAMSERVERS = -1;


class adviceserver : public vproc {
  friend void AdviceInit();

  RPC2_RequestFilter filter;
  RPC2_Handle handle;
  RPC2_PacketBuffer *packet;

  adviceserver();
  adviceserver(adviceserver&);    /* not supported! */
  operator=(adviceserver&);       /* not supported! */
  ~adviceserver();

 public:
  void main(void *);
  void CheckConnections();
};

extern int MaxAMServers;
extern void AdviceInit();

extern void NotifyUsersOfServerDownEvent(char *);
extern void NotifyUsersOfServerUpEvent(char *);
extern void NotifyUsersOfServerWeakEvent(char *);
extern void NotifyUsersOfServerStrongEvent(char *);
extern void NotifyUsersOfServerBandwidthEvent(char *, long);
extern void NotifyUsersOfHoardWalkBegin();
extern void NotifyUsersOfHoardWalkProgress(int, int);
extern void NotifyUsersOfHoardWalkEnd();
extern void NotifyUsersOfHoardWalkPeriodicOn();
extern void NotifyUsersOfHoardWalkPeriodicOff();
extern void NotifyUsersObjectInConflict(char *, ViceFid *);
extern void NotifyUsersObjectConsistent(char *, ViceFid *);
extern void NotifyUsersTaskAvailability();

extern void NotifyUserOfProgramAccess(vuid_t, int, int, ViceFid *);
extern void SwapProgramLogs();

extern void NotifyUserOfReplacement(ViceFid *, char *, int, int);
extern void SwapReplacementLogs();

#endif _VENUS_ADVICEDAEMON_H_

