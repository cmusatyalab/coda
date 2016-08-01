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

#ifndef _PACK_HELPER_
#define _PACK_HELPER_
#include <rpc2/rpc2.h>
#include <stdlib.h>
#include <string.h>

#define STUBCLIENT 0
#define STUBSERVER 1
#define STUBIN 1
#define STUBOUT 2
#define _PAD(n)((((n)-1) | 3) + 1)


int unpack_int(RPC2_Integer* new_ptr, char** old_ptr, char* EOB);

int unpack_unsigned(RPC2_Unsigned* new_ptr, char** old_ptr, char* EOB);

int unpack_double(RPC2_Double* new_ptr, char** old_ptr, char* EOB);

int unpack_bound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB, RPC2_Unsigned len);

int unpack_unbound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB);

int unpack_string(unsigned char** new_ptr, char** old_ptr, char* EOB, int mode);

int unpack_countedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        char* EOB, int mode);

int unpack_boundedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        RPC2_Unsigned* max_len_ptr, char* EOB, int mode1, int mode2);

int unpack_encryptionKey(char* new_ptr, char** old_ptr, char* EOB);

int unpack_struct_CallCountEntry(CallCountEntry* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who);

int unpack_struct_MultiCallEntry(MultiCallEntry* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who);

int unpack_struct_MultiStubWork(MultiStubWork* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who);

int pack_int(char** new_ptr, RPC2_Integer value, char* EOB);

int pack_unsigned(char** new_ptr, RPC2_Unsigned value, char* EOB);

int pack_double(char** new_ptr, RPC2_Double value, char* EOB);

int pack_bound_bytes(char** new_ptr, char* old_ptr, char* EOB, long len);

int pack_unbound_bytes(char** new_ptr, RPC2_Byte value, char* EOB);

int pack_string(char** new_ptr, char* old_ptr);

int pack_countedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned len);

int pack_boundedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len);

int pack_encryptionKey(char** new_ptr, char* old_ptr, char* EOB);

int pack_struct_CallCountEntry(char** new_ptr, CallCountEntry var, char* _EOB, RPC2_Integer who);

int pack_struct_MultiCallEntry(char** new_ptr, MultiCallEntry var, char* _EOB, RPC2_Integer who);

int pack_struct_MultiStubWork(char** new_ptr, MultiStubWork var, char* _EOB, RPC2_Integer who);

#endif



