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





#ifndef _DUMPSTREAM_H_
#define _DUMPSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <lwp.h>		/* Include all files referenced herein */
#include <voltypes.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <cvnode.h>
#include <volume.h>

#define MAXSTRLEN 80
class dumpstream {
    FILE *stream;
    char name[MAXSTRLEN];
    VnodeClass IndexType;
    int skip_vnode_garbage();
	
  public:
    dumpstream(char *);
    int getDumpHeader(struct DumpHeader *);
    int getVolDiskData(VolumeDiskData *);
    int getVnodeIndex(VnodeClass, long *, long *);
    int getNextVnode(VnodeDiskObject *, long *, int *, long *);
    int getVnode(int vnum, long unique, long offset, VnodeDiskObject *vdo);
    int copyVnodeData(DumpBuffer_t *);		/* Copy entire vnode into DumpFd*/
    int EndOfDump();				/* See if ENDDUMP is present */
    void setIndex(VnodeClass);
};

#endif	_DUMPSTREAM_H_

