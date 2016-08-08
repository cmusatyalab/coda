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


#include <rpc2/pack_helper.h>
#include <stdlib.h>
#include <string.h>


int unpack_int(BUFFER *buf, RPC2_Integer *ptr)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *ptr = ntohl(*(RPC2_Integer *)(buf->buffer));
    buf->buffer += 4;
    return 0;
}


int unpack_unsigned(BUFFER *buf, RPC2_Unsigned *ptr)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *ptr = ntohl(*(RPC2_Unsigned*)(buf->buffer));
    buf->buffer += 4;
    return 0;
}


int unpack_double(BUFFER *buf, RPC2_Double *ptr)
{
    if (buf->buffer + 8 > buf->eob)
        return -1;
    *ptr = *(RPC2_Double *)(buf->buffer);
    buf->buffer += 8;
    return 0;
}


int unpack_bound_bytes(BUFFER *buf, unsigned char *ptr, RPC2_Unsigned len)
{
    if (buf->buffer + len > buf->eob)
        return -1;
    memcpy(ptr, buf->buffer, len);
    buf->buffer += _PAD(len);
    return 0;
}

int unpack_unbound_bytes(BUFFER *buf, unsigned char *ptr)
{
    if (buf->buffer + 1 > buf->eob)
        return -1;
    *(RPC2_Byte *)ptr = *(RPC2_Byte *)(buf->buffer);
    buf->buffer += _PAD(1);
    return 0;
}


int unpack_string(BUFFER *buf, unsigned char **ptr)
{
    RPC2_Unsigned length = 0;
    if (unpack_unsigned(buf, &length))
        return -1;
    if (buf->buffer + length > buf->eob)
        return -1;
    if (*(buf->buffer + length - 1) != '\0')
        return -1;
    /* If RPC2_String is the element of RPC2_Struct, mode should be NO_MODE. */
	/* So mode should not be examined here. */
	/* if (mode == IN_OUT_MODE && who == RP2_CLIENT) { */
    assert(buf->who != RP2_CLIENT);
    /* it's very dangerous to do memcpy in client mode */
    /* if (who == RP2_CLIENT) {
        memcpy(*ptr, buf->buffer, length);
        *ptr[length] = '\0';
    */
    *ptr = (RPC2_String)(buf->buffer);
    buf->buffer += _PAD(length);
    return 0;
}


int unpack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr)
{
    RPC2_Unsigned tmp_len = 0;
    if (unpack_unsigned(buf, &tmp_len))
        return -1;
    if (buf->who == RP2_SERVER) {
	/* Special hack */
        ptr->SeqLen = tmp_len;
        if (buf->buffer + ptr->SeqLen > buf->eob)
            return -1;
        ptr->SeqBody = (RPC2_Byte *)(buf->buffer);
    } else {
        if (buf->buffer + tmp_len > buf->eob)
            return -1;
	/*    bug fix. Should update SeqLen and use select. M.K. */
	/*   fprintf(where, "
	memcpy((char *)%s->SeqBody, %s, (int32_t)%s);\n", */
        /* although currently it's not possible to unpack countedbs in client, */
        /* there is a unused function in cml.rpc2 doing such operation */
        assert(tmp_len <= ptr->SeqLen);
        ptr->SeqLen = tmp_len;
        memcpy(ptr->SeqBody, buf->buffer, ptr->SeqLen);
	/*				inc(ptr, length, where); */
    }
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}


int unpack_boundedbs(BUFFER *buf, MODE mode, RPC2_BoundedBS *ptr)
{
    if (buf->who == RP2_SERVER && mode != IN_MODE) {
        if (unpack_unsigned(buf, &ptr->MaxSeqLen))
            return -1;
    } else {
        buf->buffer += 4; /* Skip maximum length */
    }
    if ((buf->who == RP2_CLIENT && mode != IN_MODE) ||
            (buf->who == RP2_SERVER && mode != OUT_MODE)) {
        if (unpack_unsigned(buf, &ptr->SeqLen))
            return -1;
    } else if (buf->who == RP2_SERVER) {
        ptr->SeqLen = 0;
        buf->buffer += 4; /* skip packed sequence length */
    }
    if (buf->who == RP2_SERVER && mode == IN_MODE)
        ptr->MaxSeqLen = ptr->SeqLen;
    else {
        if (ptr->SeqLen > ptr->MaxSeqLen)
            return -1;
    }

    if (buf->buffer + ptr->SeqLen > buf->eob)
        return -1;
    if (buf->who == RP2_CLIENT) {
        if (mode != IN_MODE) {
            memcpy(ptr->SeqBody, buf->buffer, ptr->SeqLen);
            buf->buffer += _PAD(ptr->SeqLen);
        }
    } else {
        if (ptr->MaxSeqLen != 0) {
            ptr->SeqBody = (RPC2_String)malloc(ptr->MaxSeqLen);
            if (ptr->SeqBody == NULL)
                return -1;
            memcpy(ptr->SeqBody, buf->buffer, ptr->SeqLen);
            buf->buffer += _PAD(ptr->SeqLen);
        } else {
            ptr->SeqBody = NULL;
        }
    }
    return 0;
}


int unpack_encryptionKey(BUFFER *buf, char *ptr)
{
    if (buf->buffer + RPC2_KEYSIZE > buf->eob)
        return -1;
    memcpy(ptr, buf->buffer, RPC2_KEYSIZE);
    buf->buffer += _PAD(RPC2_KEYSIZE);
    return 0;
}


int pack_int(BUFFER *buf,  RPC2_Integer value)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Integer *)(buf->buffer) = htonl(value);
    buf->buffer += 4;
    return 0;
}


int pack_unsigned(BUFFER *buf, RPC2_Unsigned value)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Unsigned *)(buf->buffer) = htonl(value);
    buf->buffer += 4;
    return 0;
}


int pack_double(BUFFER *buf, RPC2_Double value)
{
    if (buf->buffer + 8 > buf->eob)
        return -1;
    *(RPC2_Double *)(buf->buffer) = value;
    buf->buffer += 8;
    return 0;
}


int pack_bound_bytes(BUFFER *buf, char *ptr, long len)
{
    if (buf->buffer + len > buf->eob)
        return -1;
    memcpy(buf->buffer, ptr, len);
    buf->buffer += _PAD(len);
    return 0;
}


int pack_unbound_bytes(BUFFER *buf, RPC2_Byte value)
{
    if (buf->buffer + 1 > buf->eob)
        return -1;
    *(RPC2_Byte *)(buf->buffer) = value;
    buf->buffer += _PAD(1);
    return 0;
}


int pack_string(BUFFER *buf, char *ptr)
{
    int length = strlen(ptr);
    if (pack_int(buf, length))
        return -1;
    if (buf->buffer + length + 1 > buf->eob)
        return -1;
    strcpy(buf->buffer, ptr);
    *(buf->buffer + length) = '\0';
    buf->buffer += _PAD(length + 1);
    return 0;
}


int pack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr)
{
    if (pack_unsigned(buf, ptr->SeqLen))
        return -1;
    if (buf->buffer + ptr->SeqLen > buf->eob)
        return -1;
    memcpy(buf->buffer, ptr->SeqBody, ptr->SeqLen);
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}


int pack_boundedbs(BUFFER *buf, RPC2_BoundedBS *ptr)
{
    if (pack_unsigned(buf, ptr->MaxSeqLen))
        return -1;
    if (pack_unsigned(buf, ptr->SeqLen))
        return -1;
    if (ptr->SeqLen != 0) {
        if (buf->buffer + ptr->SeqLen > buf->eob)
            return -1;
        memcpy(buf->buffer, ptr->SeqBody, ptr->SeqLen);
    }
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}


int pack_encryptionKey(BUFFER *buf, char *ptr)
{
    if (buf->buffer + RPC2_KEYSIZE > buf->eob)
        return -1;
    memcpy(buf->buffer, ptr, RPC2_KEYSIZE);
    buf->buffer += _PAD(RPC2_KEYSIZE);
    return 0;
}


int pack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr)
{
    if (pack_string(buf, (char *)ptr->name))
        return -1;
    if (pack_int(buf, ptr->countent))
        return -1;
    if (pack_int(buf, ptr->countexit))
        return -1;
    if (pack_int(buf, ptr->tsec))
        return -1;
    if (pack_int(buf, ptr->tusec))
        return -1;
    if (pack_int(buf, ptr->counttime));
        return -1;
    return 0;
}


int unpack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr)
{
    if (unpack_string(buf, &(ptr->name)))
        return -1;
    if (unpack_int(buf, &(ptr->countent)))
        return -1;
    if (unpack_int(buf, &(ptr->countexit)))
        return -1;
    if (unpack_int(buf, &(ptr->tsec)))
        return -1;
    if (unpack_int(buf, &(ptr->tusec)))
        return -1;
    if (unpack_int(buf, &(ptr->counttime)))
        return -1;
    return 0;
}


int pack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr)
{
    if (pack_string(buf, (char *)ptr->name))
        return -1;
    if (pack_int(buf, ptr->countent))
        return -1;
    if (pack_int(buf, ptr->countexit))
        return -1;
    if (pack_int(buf, ptr->tsec))
        return -1;
    if (pack_int(buf, ptr->tusec))
        return -1;
    if (pack_int(buf, ptr->counttime))
        return -1;
    return 0;
}


int unpack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr)
{
    if (unpack_string(buf, &(ptr->name)))
        return -1;
    if (unpack_int(buf, &(ptr->countent)))
        return -1;
    if (unpack_int(buf, &(ptr->countexit)))
        return -1;
    if (unpack_int(buf, &(ptr->tsec)))
        return -1;
    if (unpack_int(buf, &(ptr->tusec)))
        return -1;
    if (unpack_int(buf, &(ptr->counttime)))
        return -1;
    return 0;
}


int pack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr)
{
    if (pack_int(buf, ptr->opengate))
        return -1;
    if (pack_int(buf, ptr->tsec))
        return -1;
    if (pack_int(buf, ptr->tusec))
        return -1;
    return 0;
}


int unpack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr)
{
    if (unpack_int(buf, &(ptr->opengate)))
        return -1;
    if (unpack_int(buf, &(ptr->tsec)))
        return -1;
    if (unpack_int(buf, &(ptr->tusec)))
        return -1;
    return 0;
}

