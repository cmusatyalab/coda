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

#ifndef _ADV_DAEMON_H_
#define _ADV_DAEMON_H_

#include "adv_skk.h"
#include "advice.h"
#include "tallyent.h"
#include "user.h"
#include "venus_adv.h"
#include "venus.private.h"
#include "venus.version.h"
#include "vproc.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <rpc2/rpc2.h>
#include <rvm/rds.h>
#include <stdio.h>
#include <struct.h>
#ifdef __cplusplus
}
#endif

#define CAESUCCESS RPC2_SUCCESS /* All other CAE return codes are defined in ../rpc2/errordb.txt */
#define MAXTASKS 88

extern int SkkEnabled;
extern int ASRallowed;
extern int MaxAMServers;

class adv_daemon : public vproc {
  friend void AdviceInit();

 private:
  RPC2_RequestFilter filter;
  RPC2_Handle handle;
  RPC2_PacketBuffer *packet;

  adv_daemon();
  ~adv_daemon();

 protected:
  virtual void main(void); /* entry point */

 public:
  void CheckConnections();
};

void AdviceInit();
extern void NotifyUsersOfServerBandwidthEvent(char *, long);
extern void NotifyUsersOfServerDownEvent(char *);
extern void NotifyUsersOfServerStrongEvent(char *);
extern void NotifyUsersOfServerUpEvent(char *);
extern void NotifyUsersOfServerWeakEvent(char *);
extern void SwapProgramLogs();
extern void SwapReplacementLogs();

#endif /* _ADV_DAEMON_H_ */
