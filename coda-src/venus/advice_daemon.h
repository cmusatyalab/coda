/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
  int operator=(adviceserver&);       /* not supported! */
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

