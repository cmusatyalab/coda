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

/*
 *
 * Specification of the Venus CallBack server.
 *
 */

#ifndef _VENUSCB_H_
#define _VENUSCB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "vproc.h"

class callbackserver : public vproc {
    friend void CallBackInit();

    RPC2_RequestFilter filter;
    RPC2_Handle handle;
    RPC2_PacketBuffer *packet;

    callbackserver();
    callbackserver(callbackserver &); /* not supported! */
    int operator=(callbackserver &); /* not supported! */
    ~callbackserver();

protected:
    virtual void main(void);
};

extern int MaxCBServers;
extern int cbbreaks;
extern void CallBackInit();

#endif /* _VENUSCB_H_ */
