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





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>

#include <rpc2.h>
#include <volutil.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <util.h>
#include <callback.h>
#include <mond.h>
#include <res.h>
#include <voldump.h>
#include "velapse.h"

static void Elapse_resolution(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        if (on == 1 || on == 0) resolution_MultiStubWork[0].opengate = on;
	else CODA_ASSERT(0);
    } else if (multi == 0) {
        if (on == 1 || on == 0) resolution_ElapseSwitch = (int)on;
	else CODA_ASSERT(0);
    } else CODA_ASSERT(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: resolution %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

static void Elapse_cb(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in cb is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) cb_ElapseSwitch = (int)on;
	else CODA_ASSERT(0);
    } else CODA_ASSERT(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: cb %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

static void Elapse_mond(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in mond is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) mond_ElapseSwitch = (int)on;
	else CODA_ASSERT(0);
    } else CODA_ASSERT(0);
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: mond %s %s", (on == 1) ? "ON" : "OFF", (multi == 1) ? "for MultiRPC" : "");
}    

static void Elapse_volDump(RPC2_Integer on, RPC2_Integer multi)
{
    if (multi == 1) {
        LogMsg(0, VolDebugLevel, stdout, "VolElapse: MultiRPC in volDump is not supported");
	return;
    } else if (multi == 0) {
        if (on == 1 || on == 0) volDump_ElapseSwitch = (int)on;
	else CODA_ASSERT(0);
    } else CODA_ASSERT(0);
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



