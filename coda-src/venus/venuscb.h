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
 * Specification of the Venus CallBack server.
 *
 */


#ifndef	_VENUSCB_H_
#define _VENUSCB_H_


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from venus */
#include "vproc.h"


const int DFLT_MAXCBSERVERS = 5;
const int UNSET_MAXCBSERVERS = -1;


class callbackserver : public vproc {
  friend void CallBackInit();

    RPC2_RequestFilter filter;
    RPC2_Handle handle;
    RPC2_PacketBuffer *packet;

    callbackserver();
    callbackserver(callbackserver&);	/* not supported! */
    int operator=(callbackserver&);		/* not supported! */
    ~callbackserver();

  protected:
    virtual void main(void);
};

extern int MaxCBServers;
extern int cbbreaks;
extern void CallBackInit();

#endif not _VENUSCB_H_
