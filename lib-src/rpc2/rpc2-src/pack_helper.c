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
#include <assert.h>

#define _PAD(n)((((n)-1) | 3) + 1)

/* because every RPC2 message is sent as a UDP packet we should never
 * need to allocate more than a UDP packet size's worth of data and even
 * that is generous because we try to avoid ip/ipv6 fragmentation */
#define ALLOC_MAX 65507

int pack_int(BUFFER *buf,  RPC2_Integer value)
{
    if (buf->eob) {
        if (buf->buffer + 4 > buf->eob)
            return -1;
        *(RPC2_Integer *)(buf->buffer) = htonl(value);
    }
    buf->buffer += 4;
    return 0;
}

int unpack_int(BUFFER *buf, RPC2_Integer *ptr)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *ptr = ntohl(*(RPC2_Integer *)(buf->buffer));
    buf->buffer += 4;
    return 0;
}


int pack_unsigned(BUFFER *buf, RPC2_Unsigned value)
{
    if (buf->eob) {
        if (buf->buffer + 4 > buf->eob)
            return -1;
        *(RPC2_Unsigned *)(buf->buffer) = htonl(value);
    }
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


int pack_double(BUFFER *buf, RPC2_Double value)
{
    if (buf->eob) {
        if (buf->buffer + 8 > buf->eob)
            return -1;
        *(RPC2_Double *)(buf->buffer) = value;
    }
    buf->buffer += 8;
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


int pack_bytes(BUFFER *buf, RPC2_ByteSeq value, RPC2_Unsigned len)
{
    if (buf->eob) {
        if (buf->buffer + len > buf->eob)
            return -1;
        memcpy(buf->buffer, value, len);
    }
    buf->buffer += _PAD(len);
    return 0;
}

int unpack_bytes(BUFFER *buf, RPC2_ByteSeq ptr, RPC2_Unsigned len)
{
    if (buf->buffer + len > buf->eob)
        return -1;
    memcpy(ptr, buf->buffer, len);
    buf->buffer += _PAD(len);
    return 0;
}


int pack_byte(BUFFER *buf, RPC2_Byte value)
{
    if (buf->eob) {
        if (buf->buffer + 1 > buf->eob)
            return -1;
        *(RPC2_Byte *)(buf->buffer) = value;
    }
    buf->buffer += _PAD(1);
    return 0;
}

int unpack_byte(BUFFER *buf, RPC2_Byte *ptr)
{
    if (buf->buffer + 1 > buf->eob)
        return -1;
    *(RPC2_Byte *)ptr = *(RPC2_Byte *)(buf->buffer);
    buf->buffer += _PAD(1);
    return 0;
}


int pack_string(BUFFER *buf, RPC2_String ptr)
{
    int length = strlen((const char *)ptr);

    if (pack_int(buf, length))
        return -1;

    if (buf->eob) {
        if (buf->buffer + length + 1 > buf->eob)
            return -1;
        strcpy(buf->buffer, (const char *)ptr);
        *(buf->buffer + length) = '\0';
    }
    buf->buffer += _PAD(length + 1);
    return 0;
}

int unpack_string(BUFFER *buf, RPC2_String *ptr)
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


int pack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr)
{
    if (pack_unsigned(buf, ptr->SeqLen))
        return -1;

    if (buf->eob) {
        if (buf->buffer + ptr->SeqLen > buf->eob)
            return -1;
        memcpy(buf->buffer, ptr->SeqBody, ptr->SeqLen);
    }
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}

int unpack_countedbs(BUFFER *buf, RPC2_CountedBS *ptr)
{
    if (unpack_unsigned(buf, &ptr->SeqLen))
        return -1;

    if (buf->buffer + ptr->SeqLen > buf->eob)
        return -1;
    ptr->SeqBody = (RPC2_Byte *)(buf->buffer);

    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}


int pack_boundedbs(BUFFER *buf, RPC2_BoundedBS *ptr)
{
    if (pack_unsigned(buf, ptr->MaxSeqLen))
        return -1;
    if (pack_unsigned(buf, ptr->SeqLen))
        return -1;

    if (buf->eob) {
        if (ptr->SeqLen != 0) {
            if (buf->buffer + ptr->SeqLen > buf->eob)
                return -1;
            memcpy(buf->buffer, ptr->SeqBody, ptr->SeqLen);
        }
    }
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}

int unpack_boundedbs(BUFFER *buf, MODE mode, RPC2_BoundedBS *ptr)
{
    unsigned int maxseqlen;
    unsigned int seqlen;

    if (unpack_unsigned(buf, &maxseqlen))
        return -1;
    if (unpack_unsigned(buf, &seqlen))
        return -1;

    if (buf->who == RP2_SERVER) {
        if (mode == IN_MODE) {
            ptr->MaxSeqLen = seqlen;
        } else {
            ptr->MaxSeqLen = maxseqlen;
        }
    }
    if ((buf->who == RP2_CLIENT && mode != IN_MODE) ||
        (buf->who == RP2_SERVER && mode != OUT_MODE)) {
        ptr->SeqLen = seqlen;
    } else {
        ptr->SeqLen = 0;
    }

    if (ptr->SeqLen > ptr->MaxSeqLen)
        return -1;
    if (buf->buffer + ptr->SeqLen > buf->eob)
        return -1;

    if (buf->who == RP2_SERVER) {
        if (ptr->MaxSeqLen > ALLOC_MAX)
            return -1;

        ptr->SeqBody = calloc(1, ptr->MaxSeqLen);
        if (ptr->MaxSeqLen && ptr->SeqBody == NULL)
            return -1;
    }

    memcpy(ptr->SeqBody, buf->buffer, ptr->SeqLen);
    buf->buffer += _PAD(ptr->SeqLen);
    return 0;
}


int pack_encryptionKey(BUFFER *buf, char *ptr)
{
    if (buf->eob) {
        if (buf->buffer + RPC2_KEYSIZE > buf->eob)
            return -1;
        memcpy(buf->buffer, ptr, RPC2_KEYSIZE);
    }
    buf->buffer += _PAD(RPC2_KEYSIZE);
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


int pack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr)
{
    if (pack_string(buf, ptr->name))
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
    if (pack_string(buf, ptr->name))
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

