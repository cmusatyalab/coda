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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <libc.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <callback.h>
#include <mond.h>
#include <res.h>
#include <voldump.h>
#include "velapse.h"

PRIVATE void Elapse_resolution(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        if (on == 1 || on == 0) resolution_MultiStubWork[0].opengate = on;
	else assert(0);
    } else if (multi == 0) {
        if (on == 1 || on == 0) resolution_ElapseSwitch = (int)on;
	else assert(0);
    } else assert(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: resolution %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

PRIVATE void Elapse_cb(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in cb is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) cb_ElapseSwitch = (int)on;
	else assert(0);
    } else assert(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: cb %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

PRIVATE void Elapse_mond(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in mond is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) mond_ElapseSwitch = (int)on;
	else assert(0);
    } else assert(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: mond %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

PRIVATE void Elapse_volDump(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in volDump is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) volDump_ElapseSwitch = (int)on;
	else assert(0);
    } else assert(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: volDump %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

/*
  BEGIN_HTML
  <a name="S_VolElapse"><strong>Print out elapsed time for specified activity</strong></a>
  END_HTML
*/
long S_VolElapse(RPC2_Handle rpcid, RPC2_Integer on, RPC2_Integer subid, RPC2_Integer multi)
{
    switch (subid) {
        case VELAPSE_RESOLUTION:
                   Elapse_resolution(on, multi);
		   break;
        case VELAPSE_CB:
                   Elapse_cb(on, multi);
		   break;
        case VELAPSE_MOND:
                   Elapse_mond(on, multi);
		   break;
        case VELAPSE_VOLDUMP:
                   Elapse_volDump(on, multi);
		   break;
        default:
                   LogMsg(0, VolDebugLevel, stdout, "Illegal switch name");
                   break;
    }

    return RPC2_SUCCESS;
}



