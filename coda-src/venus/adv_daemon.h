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

#ifndef _ADV_DAEMON_H_
#define _ADV_DAEMON_H_

#include "adv_skk.h"
#include "advice.h"
#include "tallyent.h"
#include "user.h"
#include "venus_adv.h"
#include "venus.private.h"
#include "vproc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netinet/in.h>
#include <rpc2/rpc2.h>
#include <rvm/rds.h>
#include <stdio.h>
#include <struct.h>
#ifdef __cplusplus
}
#endif

extern int SkkEnabled;
extern int ASRallowed;
extern int MaxAMServers;

class adv_daemon : public vproc {
  friend void AdviceInit();

 private:
  RPC2_RequestFilter filter;
  RPC2_Handle handle;
  RPC2_PacketBuffer *packet;

  adv_daemon(void);
  ~adv_daemon(void);

 protected:
  virtual void main(void); /* entry point */

 public:
  void CheckConnections();
};

void AdviceInit();

#endif /* _ADV_DAEMON_H_ */
