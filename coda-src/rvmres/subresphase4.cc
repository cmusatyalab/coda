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
#include <rpc2.h>
#include <util.h>
#include <codadir.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <olist.h>
#include <dlist.h>
#include <cvnode.h>
#include <vcrcommon.h>
#include <vlist.h>
#include <vrdb.h>
#include <srv.h>
#include <res.h>
#include <operations.h>
#include <resutil.h>
#include <reslog.h>
#include <remotelog.h>
#include <treeremove.h>
#include <timing.h>
#include "rsle.h"
#include "parselog.h"
#include "compops.h"
#include "ruconflict.h"
#include "ops.h"
#include "rvmrestiming.h"
#include "resstats.h"

long RS_DirResPhase3(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
		  SE_Descriptor *sed);

long RS_ResPhase4(RPC2_Handle RPCid, ViceFid *Fid, ViceVersionVector *VV,
		  SE_Descriptor *sed) {

    /* corresponds to phase 3 of old res subsystem in VM */
    return(RS_DirResPhase3(RPCid, Fid, VV, sed));
}
    
    
