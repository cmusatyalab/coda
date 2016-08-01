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

#include <rpc2/pack_helper.h>
#include <rpc2/rpc2.h>
#include <stdlib.h>
#include <string.h>


int unpack_int(RPC2_Integer* new_ptr, char** old_ptr, char* EOB)
{
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Integer *)(*old_ptr));
    *old_ptr += 4;
    return 0;
}


int unpack_unsigned(RPC2_Unsigned* new_ptr, char** old_ptr, char* EOB)
{
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Unsigned*)old_ptr);
    *old_ptr += 4;
    return 0;
}


int unpack_double(RPC2_Double* new_ptr, char** old_ptr, char* EOB)
{
    if (*old_ptr + 8 > EOB)
        return -1;
    *new_ptr = *(RPC2_Double *)old_ptr;
    *old_ptr += 8;
    return 0;
}


int unpack_bound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB, RPC2_Unsigned len)
{
    if (*old_ptr + len > EOB)
        return -1;
    memcpy(new_ptr, *old_ptr, len);
    *old_ptr += _PAD(len);
    return 0;
}

int unpack_unbound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB)
{
    if (*old_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = *(*(RPC2_Byte **)old_ptr);
    *old_ptr += 4;
    return 0;
}


int unpack_string(unsigned char** new_ptr, char** old_ptr, char* EOB, int mode)
{
    if (*old_ptr + 4 > EOB)
        return -1;

    RPC2_Unsigned length = 1 + ntohl(*(RPC2_Integer *)(*old_ptr));
    *old_ptr += 4;
    if (*old_ptr + _PAD(length) > EOB)
        return -1;
    if (*(*old_ptr + length - 1) != '\0')
        return -1;
	/* If RPC2_String is the element of RPC2_Struct, mode should be NO_MODE. */
	/* So mode should not be examined here. */
	/* if (mode == IN_OUT_MODE && who == RP2_CLIENT) { */
    if (mode == STUBCLIENT) {
		/* Just copy characters back */
        memcpy(*new_ptr, *old_ptr, length);
        *new_ptr[length] = '\0';
    } else {
		/* After the above condition check, the following never occurs.. */
		/* if (mode != NO_MODE && who == RP2_CLIENT) fputc('*', where); */
        *new_ptr = (RPC2_String)*old_ptr;
    }
    *old_ptr += _PAD(length);
    return 0;
}


int unpack_countedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        char* EOB, int mode)
{
    if (*old_ptr + 4 > EOB)
        return -1;
    if (mode == STUBSERVER) {
		/* Special hack */
        *len_ptr = ntohl(*(RPC2_Integer*)(*old_ptr));
        *old_ptr += 4;
        if (*old_ptr + _PAD(*len_ptr) > EOB)
            return -1;
        *new_ptr = (RPC2_Byte *)(*old_ptr);
        *old_ptr += _PAD(*len_ptr);
        return 0;
    } else {
        *len_ptr = ntohl(*(RPC2_Integer*)(*old_ptr));
        *old_ptr += 4;
        if (*old_ptr + _PAD(*len_ptr) > EOB)
            return -1;
		/*    bug fix. Should update SeqLen and use select. M.K. */
		/*   fprintf(where, "
		    memcpy((char *)%s->SeqBody, %s, (int32_t)%s);\n", */
        memcpy(*new_ptr, *old_ptr, *len_ptr);
		/*				inc(ptr, length, where); */
        *old_ptr += _PAD(*len_ptr);
        return 0;
    }
}


int unpack_boundedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        RPC2_Unsigned* max_len_ptr, char* EOB, int mode1, int mode2)
{
    if (*old_ptr + 8 > EOB)
        return -1;
    if (mode1 == STUBSERVER && mode2 != STUBIN) {
        *max_len_ptr = ntohl(*(RPC2_Unsigned *)(*old_ptr));
    }
    *old_ptr += 4; /* Skip maximum length */
    if ((mode1 == STUBCLIENT && mode2 != STUBIN) ||
            (mode1 == STUBSERVER && mode2 != STUBOUT)) {
        *len_ptr = ntohl(*(RPC2_Unsigned *)(*old_ptr));
    } else if (mode1 == STUBSERVER)
        *len_ptr = 0;
    *old_ptr += 4; /* skip packed sequence length */
    if (mode1 == STUBSERVER && mode2 == STUBIN)
        *max_len_ptr = *len_ptr;
    else {
        if (*len_ptr > *max_len_ptr)
            return -1;
    }

    if (*old_ptr + _PAD(*len_ptr) > EOB)
        return -1;
    if (mode1 == STUBCLIENT) {
        if (mode2 != STUBIN) {
            memcpy((*new_ptr), *old_ptr, *len_ptr);
            *old_ptr += _PAD(*len_ptr);
        }
    } else {
        if (*max_len_ptr != 0) {
            *new_ptr = (RPC2_String)malloc(*max_len_ptr);
            if (*new_ptr == NULL)
                return -1;
            memcpy((*new_ptr), *old_ptr, *len_ptr);
            *old_ptr += _PAD(*len_ptr);
        } else {
            *new_ptr = NULL;
        }
    }
    return 0;

}


int unpack_encryptionKey(char* new_ptr, char** old_ptr, char* EOB)
{
    if (*old_ptr + _PAD(RPC2_KEYSIZE) > EOB)
        return -1;
    memcpy(new_ptr, *old_ptr, RPC2_KEYSIZE);
    *old_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}


int pack_int(char** new_ptr, RPC2_Integer value, char* EOB)
{
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Integer *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


int pack_unsigned(char** new_ptr, RPC2_Unsigned value, char* EOB)
{
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Unsigned *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


int pack_double(char** new_ptr, RPC2_Double value, char* EOB)
{
    if (*new_ptr + 8 > EOB)
        return -1;
    *(RPC2_Double *)new_ptr = value;
    *new_ptr += 8;
    return 0;
}


int pack_bound_bytes(char** new_ptr, char* old_ptr, char* EOB, long len)
{
    if (*new_ptr + len > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, len);
    *new_ptr += _PAD(len);
    return 0;
}


int pack_unbound_bytes(char** new_ptr, RPC2_Byte value, char* EOB)
{
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = value;
    *new_ptr += 4;
    return 0;
}


int pack_string(char** new_ptr, char* old_ptr)
{
    int length = strlen(old_ptr);
    *(*(RPC2_Integer **)(new_ptr)) = length;
    strcpy(*new_ptr + 4, old_ptr);
    *(*new_ptr + 4 + length) = '\0';
    *new_ptr += 4 + _PAD(length + 1);
    return 0;
}


int pack_countedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned len)
{
    *(*(RPC2_Unsigned **)new_ptr) = htonl(len);
    memcpy(*new_ptr + 4, old_ptr, len);
    *new_ptr += 4 + _PAD(len);
    return 0;
}


int pack_boundedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len)
{
    *(*(RPC2_Unsigned **)new_ptr) = htonl(maxLen);
    if (len == 0)
        *(RPC2_Unsigned *)(*new_ptr + 4) = 0;
    else {
      *(RPC2_Unsigned *)(*new_ptr + 4) = htonl(len);
      memcpy(*new_ptr + 8, old_ptr, len);
    }
    *new_ptr += 8 + _PAD(len);
    return 0;
}


int pack_encryptionKey(char** new_ptr, char* old_ptr, char* EOB)
{
    if (*new_ptr + RPC2_KEYSIZE > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, RPC2_KEYSIZE);
    *new_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}


int pack_struct_CallCountEntry(char** new_ptr, CallCountEntry var, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = pack_string(new_ptr, (char*)var.name);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.countent, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.countexit, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tsec, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tusec, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.counttime, _EOB);
    if (rc < 0) return -1;
    return 0;
}


int unpack_struct_CallCountEntry(CallCountEntry* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = unpack_string((unsigned char**)&((*new_ptr).name), old_ptr, _EOB, 0);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).countent, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).countexit, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tsec, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tusec, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).counttime, old_ptr, _EOB);
    if (rc < 0) return -1;
    return 0;
}


int pack_struct_MultiCallEntry(char** new_ptr, MultiCallEntry var, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = pack_string(new_ptr, (char*)var.name);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.countent, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.countexit, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tsec, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tusec, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.counttime, _EOB);
    if (rc < 0) return -1;
    return 0;
}


int unpack_struct_MultiCallEntry(MultiCallEntry* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = unpack_string((unsigned char**)&((*new_ptr).name), old_ptr, _EOB, 0);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).countent, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).countexit, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tsec, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tusec, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).counttime, old_ptr, _EOB);
    if (rc < 0) return -1;
    return 0;
}


int pack_struct_MultiStubWork(char** new_ptr, MultiStubWork var, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = pack_int(new_ptr, var.opengate, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tsec, _EOB);
    if (rc < 0) return -1;
    rc = pack_int(new_ptr, var.tusec, _EOB);
    if (rc < 0) return -1;
    return 0;
}


int unpack_struct_MultiStubWork(MultiStubWork* new_ptr, char** old_ptr, char* _EOB, RPC2_Integer who)
{
    int rc = 0;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).opengate, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tsec, old_ptr, _EOB);
    if (rc < 0) return -1;
    rc = unpack_int((RPC2_Integer *)&(*new_ptr).tusec, old_ptr, _EOB);
    if (rc < 0) return -1;
    return 0;
}

