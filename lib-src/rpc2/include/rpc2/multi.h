/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#include <rpc2/rpc2.h>
#include <rpc2/se.h>

#ifndef _MULTI_H_
#define _MULTI_H_

#define MAXSERVERS 100

typedef long RPC2_HandleResult_func(int HowMany, RPC2_Handle ConnList[],
                                    long offset, long rpcval, ...);

/* union for packing and unpacking unspecified arguments (identified by
 * parallel ARG structure
 */
typedef union PARM { /* PARM will always be 4 bytes */
    RPC2_Integer integer;
    RPC2_Integer **integerp;
    RPC2_Unsigned unsgned;
    RPC2_Unsigned **unsgnedp;
    RPC2_Byte byte;
    RPC2_Byte **bytep;
    RPC2_String string;
    RPC2_String **stringp;
    RPC2_CountedBS *cbs;
    RPC2_CountedBS **cbsp;
    RPC2_BoundedBS *bbs;
    RPC2_BoundedBS **bbsp;
    RPC2_EncryptionKey *key;
    RPC2_EncryptionKey **keyp;
    RPC2_Handle *cidp;
    SE_Descriptor *sedp;
    union PARM *structp;
    union PARM **structpp;
} PARM;

/* used to pass information through RPC2_MultiRPC() call */
typedef struct arg_info {
    ARG *ArgTypes;
    PARM *Args;
    RPC2_HandleResult_func *HandleResult;
    int ArgCount;
} ARG_INFO;

/* Macros to simplify use of MultiRPC; these used to be
   in files venus/comm.h and res/rescomm.h (duplicated!)
   in Coda; moved them here since they are really not
   Coda-specific (Satya, 5/23/95) */

#define ARG_MARSHALL(mode, type, name, object, howmany)                 \
    type *name##_ptrs[howmany] __attribute__((unused));                 \
    type name##_bufs[howmany];                                          \
    {                                                                   \
        memset(&name##_bufs, 0, sizeof(type) * howmany);                \
        for (unsigned int name##_local_i = 0; name##_local_i < howmany; \
             name##_local_i++) {                                        \
            name##_ptrs[name##_local_i] = &name##_bufs[name##_local_i]; \
            if (mode == IN_OUT_MODE)                                    \
                name##_bufs[name##_local_i] = (object);                 \
        }                                                               \
    }

#define ARG_MARSHALL_BS(mode, type, name, object, howmany, maxbslen)          \
    type *name##_ptrs[howmany];                                               \
    type name##_bufs[howmany];                                                \
    char name##_data[maxbslen * howmany];                                     \
    {                                                                         \
        for (int name##_local_i = 0; name##_local_i < howmany;                \
             name##_local_i++) {                                              \
            name##_ptrs[name##_local_i] = &name##_bufs[name##_local_i];       \
            if (mode == OUT_MODE)                                             \
                (object).SeqLen = 0;                                          \
            name##_bufs[name##_local_i] = (object);                           \
            name##_bufs[name##_local_i].SeqBody =                             \
                (RPC2_ByteSeq)&name##_data[name##_local_i * maxbslen];        \
            if ((object).SeqLen > 0)                                          \
                memcpy(name##_bufs[name##_local_i].SeqBody, (object).SeqBody, \
                       (int)(object).SeqLen);                                 \
        }                                                                     \
    }

#define ARG_MARSHALL_ARRAY(mode, type, name, numelts, maxelts, object,   \
                           howmany)                                      \
    type *name##_ptrs[howmany];                                          \
    type name##_bufs[howmany][maxelts]; /* maxelts must be a constant */ \
    {                                                                    \
        for (unsigned int name##_local_i = 0; name##_local_i < howmany;  \
             name##_local_i++) {                                         \
            name##_ptrs[name##_local_i] = name##_bufs[name##_local_i];   \
            if (mode == IN_OUT_MODE) {                                   \
                for (unsigned int name##_local_j = 0;                    \
                     name##_local_j < numelts; name##_local_j++)         \
                    name##_bufs[name##_local_i][name##_local_j] =        \
                        (object)[name##_local_j];                        \
            }                                                            \
        }                                                                \
    }

#define ARG_UNMARSHALL(name, object, ix) (object) = name##_bufs[ix];

#define ARG_UNMARSHALL_BS(name, object, ix)                                 \
    {                                                                       \
        RPC2_Integer seqlen = name##_bufs[ix].SeqLen;                       \
        (object).SeqLen     = seqlen;                                       \
        if (seqlen > 0)                                                     \
            memcpy((object).SeqBody, name##_bufs[ix].SeqBody, (int)seqlen); \
    }

#define ARG_UNMARSHALL_ARRAY(name, numelts, object, ix)                   \
    {                                                                     \
        for (unsigned int name##_local_i = 0; name##_local_i < (numelts); \
             name##_local_i++)                                            \
            (object)[name##_local_i] = name##_bufs[ix][name##_local_i];   \
    }

#endif /* _MULTI_H_ */
