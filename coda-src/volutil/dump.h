/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _DUMP_H_
#define _DUMP_H_ 1

#define DUMPVERSION 2

#define DUMPENDMAGIC 0x3A214B6E
#define DUMPBEGINMAGIC 0xB3A11322

#define D_DUMPHEADER 1
#define D_VOLUMEHEADER 2
#define D_SMALLINDEX 3
#define D_LARGEINDEX 4
#define D_VNODE 5
#define D_VV 6
#define D_ENDVV 7
#define D_DUMPEND 8
#define D_VOLUMEDISKDATA 9
#define D_NULLVNODE 10
#define D_DIRPAGES 11
#define D_FILEDATA 12
#define D_RMVNODE 13
#define D_BADINODE 14
#define D_MAX 20

#define MAXDUMPTIMES 50

struct DumpHeader {
    int version;
    VolumeId volumeId;
    VolumeId parentId;
    char volumeName[V_MAXVOLNAMELEN];
    unsigned int Incremental;
    Date_t backupDate;
    unsigned int oldest, latest;
};

typedef struct {
    char *DumpBuf; /* Start of buffer for spooling */
    char *DumpBufPtr; /* Current position in buffer */
    char *DumpBufEnd; /* End of buffer */
    unsigned long offset; /* Number of bytes read or written so far. */
    RPC2_Handle rpcid; /* RPCid of connection, NULL if dumping to file */
    int DumpFd; /* fd to which to flush or VolId if using RPC */
    unsigned long nbytes; /* Count of total bytes transferred. */
    unsigned long secs; /* Elapsed time for transfers -- not whole op */
} DumpBuffer_t;
#define VOLID DumpFd /* Overload this field if using newstyle dump */

/* Exported Routines (from dumpstuff.c) */
extern DumpBuffer_t *InitDumpBuf(char *buf, long size, VolumeId volid,
                                 RPC2_Handle rpcid);
extern DumpBuffer_t *InitDumpBuf(char *buf, long size, int fd);
extern int DumpDouble(DumpBuffer_t *, char, unsigned int, unsigned int);
extern int DumpInt32(DumpBuffer_t *, char tag, unsigned int value);
extern int DumpByte(DumpBuffer_t *, char tag, char value);
extern int DumpBool(DumpBuffer_t *, char tag, unsigned int value);
extern int DumpArrayInt32(DumpBuffer_t *, char, unsigned int *, int nelem);
extern int DumpShort(DumpBuffer_t *, char tag, unsigned int value);
extern int DumpString(DumpBuffer_t *, char tag, char *s);
extern int DumpByteString(DumpBuffer_t *, char tag, char *bs, int nbytes);
extern int DumpVV(DumpBuffer_t *, char tag, struct ViceVersionVector *vv);
extern int DumpFile(DumpBuffer_t *, char tag, int fd, int vnode);
extern int DumpTag(DumpBuffer_t *, char tag);
extern int DumpEnd(DumpBuffer_t *);

/* Exported Routines (from readstuff.c) */
extern signed char ReadTag(DumpBuffer_t *);
extern int PutTag(char, DumpBuffer_t *);
extern int ReadShort(DumpBuffer_t *, unsigned short *sp);
extern int ReadInt32(DumpBuffer_t *, unsigned int *lp);
extern int ReadString(DumpBuffer_t *, char *to, int max);
extern int ReadByteString(DumpBuffer_t *, char *to, int size);
extern int ReadDumpHeader(DumpBuffer_t *, struct DumpHeader *hp);
extern int ReadVolumeDiskData(DumpBuffer_t *, VolumeDiskData *vol);
extern int ReadVV(DumpBuffer_t *, ViceVersionVector *vv);
extern int ReadFile(DumpBuffer_t *, FILE *);
extern int EndOfDump(DumpBuffer_t *);

#endif /* _DUMP_H_ */
