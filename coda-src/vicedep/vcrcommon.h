/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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

#ifndef _VCRCOMMON_
#define _VCRCOMMON_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
#include "rpc2.h"
#include "se.h"
#include "errors.h"
#ifdef __cplusplus
}
#endif __cplusplus


#ifndef _FID_T_
#define _FID_T_

typedef RPC2_Unsigned VolumeId;

typedef RPC2_Unsigned VnodeId;

typedef VolumeId VolId;

typedef RPC2_Unsigned Unique_t;

typedef RPC2_Unsigned FileVersion;
 
#endif


#ifndef _VUID_T_
#define _VUID_T_
typedef unsigned int vuid_t;
typedef unsigned int vgid_t;
#endif _VUID_T_


#ifndef _VICEFID_T_
#define _VICEFID_T_


typedef struct ViceFid {
    RPC2_Unsigned Volume;
    RPC2_Unsigned Vnode;
    RPC2_Unsigned Unique;
} ViceFid;
 
#endif


typedef struct ViceStoreId {
    RPC2_Unsigned Host;
    RPC2_Unsigned Uniquifier;
} ViceStoreId;

typedef struct ViceVersionArray {
    RPC2_Integer Site0;
    RPC2_Integer Site1;
    RPC2_Integer Site2;
    RPC2_Integer Site3;
    RPC2_Integer Site4;
    RPC2_Integer Site5;
    RPC2_Integer Site6;
    RPC2_Integer Site7;
} ViceVersionArray;

typedef struct ViceVersionVector {
    ViceVersionArray Versions;
    ViceStoreId StoreId;
    RPC2_Unsigned Flags;
} ViceVersionVector;

typedef RPC2_Unsigned UserId;

typedef RPC2_Unsigned Date_t;

typedef RPC2_Integer Rights;

typedef enum{ Invalid=0, File=1, Directory=2, SymbolicLink=3 } ViceDataType;

typedef enum{ NoCallBack=0, CallBackSet=1 } CallBackStatus;

typedef enum{ NoPermit=0, PermitSet=1 } PermitStatus;

typedef struct ViceStatus {
    RPC2_Unsigned InterfaceVersion;
    ViceDataType VnodeType;
    RPC2_Integer LinkCount;
    RPC2_Unsigned Length;
    FileVersion DataVersion;
    ViceVersionVector VV;
    Date_t Date;
    UserId Author;
    UserId Owner;
    CallBackStatus CallBack;
    Rights MyAccess;
    Rights AnyAccess;
    RPC2_Unsigned Mode;
    VnodeId vparent;
    Unique_t uparent;
} ViceStatus;

#ifndef _STUB_PREDEFINED_
#define _STUB_PREDEFINED_

typedef struct CallCountEntry {
    RPC2_String name;
    RPC2_Integer countent;
    RPC2_Integer countexit;
    RPC2_Integer tsec;
    RPC2_Integer tusec;
    RPC2_Integer counttime;
} CallCountEntry;

typedef struct MultiCallEntry {
    RPC2_String name;
    RPC2_Integer countent;
    RPC2_Integer countexit;
    RPC2_Integer tsec;
    RPC2_Integer tusec;
    RPC2_Integer counttime;
    RPC2_Integer counthost;
} MultiCallEntry;

typedef struct MultiStubWork {
    RPC2_Integer opengate;
    RPC2_Integer tsec;
    RPC2_Integer tusec;
} MultiStubWork;
#endif _STUB_PREDEFINED_

/* Op codes and definitions */

#ifdef __cplusplus
extern "C"{
#endif
#ifdef __cplusplus
}
#endif

#endif _VCRCOMMON_
