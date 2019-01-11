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

#ifndef _DUMPSTREAM_H_
#define _DUMPSTREAM_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Enable 64-bit file offsets */
/* eventually this should be used everywhere */
#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <lwp/lwp.h> /* Include all files referenced herein */
#include <lwp/lock.h>
#include <voltypes.h>

#ifdef __cplusplus
}
#endif

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
    ~dumpstream();
    int isopen(); /* 1 if dumpstream is open; 0 otherwise */
    int getDumpHeader(struct DumpHeader *);
    int getVolDiskData(VolumeDiskData *);
    int getVnodeIndex(VnodeClass, long *, long *);
    int getNextVnode(VnodeDiskObject *, VnodeId *, int *, off_t *offset);
    int getVnode(int vnum, long unique, off_t offset, VnodeDiskObject *vdo);
    int copyVnodeData(DumpBuffer_t *); /* Copy entire vnode into DumpFd*/
    int EndOfDump(); /* See if ENDDUMP is present */
    void setIndex(VnodeClass);
    int readDirectory(PDirInode *);
    int CopyBytesToMemory(char *, int);
    int CopyBytesToFile(FILE *, int);
};

/* Debugging routine: prints dump header on specified output file */
void PrintDumpHeader(FILE *, struct DumpHeader *);

#endif /* _DUMPSTREAM_H_ */
