/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.


#*/


#ifndef _PACK_HELPER_
#define _PACK_HELPER_
#include <rpc2/rpc2.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define _PAD(n)((((n)-1) | 3) + 1)
typedef struct {
    char* buffer;
    char* eob;
    WHO who;
} BUFFER;



int unpack_int(BUFFER *buf, RPC2_Integer *ptr);

int unpack_unsigned(BUFFER *buf, RPC2_Unsigned *ptr);

int unpack_double(BUFFER *buf, RPC2_Double *ptr);

int unpack_bound_bytes(BUFFER *buf, unsigned char *ptr, RPC2_Unsigned len);

int unpack_unbound_bytes(BUFFER *buf, unsigned char *ptr);

int unpack_string(BUFFER *buf, unsigned char **ptr);

int unpack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr);

int unpack_boundedbs(BUFFER *buf, MODE mode, RPC2_BoundedBS *ptr);

int unpack_encryptionKey(BUFFER *buf, char *ptr);

int unpack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr);

int unpack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr);

int unpack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr);

int pack_int(BUFFER *buf, RPC2_Integer value);

int pack_unsigned(BUFFER *buf, RPC2_Unsigned value);

int pack_double(BUFFER *buf, RPC2_Double value);

int pack_bound_bytes(BUFFER *buf, char *ptr, long len);

int pack_unbound_bytes(BUFFER *buf, RPC2_Byte value);

int pack_string(BUFFER *buf, char *ptr);

int pack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr);

int pack_boundedbs(BUFFER *buf, RPC2_BoundedBS *ptr);

int pack_encryptionKey(BUFFER *buf, char *ptr);

int pack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr);

int pack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr);

int pack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr);

#endif



