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

#ifndef _RESCOORD_H_
#define _RESCOORD_H_

int IsWeaklyEqual(ViceVersionVector **VV, int nvvs);
int WEResPhase1(ViceFid *Fid, ViceVersionVector **VV,
		res_mgrpent *mgrp, unsigned long *hosts,
		ViceStoreId *stid, ResStatus **rstatusp);
int CompareDirContents(SE_Descriptor *sid_bufs, ViceFid *fid);
int RegDirResolution(res_mgrpent *mgrp, ViceFid *Fid, ViceVersionVector **VV,
		     ResStatus **rstatusp, int *logresreq);

#endif /* _RESCOORD_H_ */
