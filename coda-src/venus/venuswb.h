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
 * Specification of the Venus writeback server.
 *
 */


#ifndef	_VENUSWB_H_
#define _VENUSWB_H_


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

/* from venus */
#include "vproc.h"


const int DFLT_MAXWBSERVERS = 5;
const int UNSET_MAXWBSERVERS = -1;

class writebackserver : public vproc {
    friend void WritebackInit();

    RPC2_RequestFilter filter;
    RPC2_Handle handle;
    RPC2_PacketBuffer *packet;

    writebackserver();
    writebackserver(writebackserver&); // not supported!
    int operator=(writebackserver&); // not supported!
    ~writebackserver();

  protected:
    virtual void main(void);
};

extern int MaxWBServers;
extern void WritebackInit();

#endif not _VENUSWB_H_
