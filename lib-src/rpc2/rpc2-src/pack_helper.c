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
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Byte *)ptr = *(RPC2_Byte *)(buf->buffer);
    buf->buffer += 4;
    return 0;
}


int unpack_string(BUFFER *buf, unsigned char **ptr, int who)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;

    RPC2_Unsigned length = 1 + ntohl(*(RPC2_Integer *)(buf->buffer));
    (buf->buffer) += 4;
    if (buf->buffer + _PAD(length) > buf->eob)
        return -1;
    if (*(buf->buffer + length - 1) != '\0')
        return -1;
    /* If RPC2_String is the element of RPC2_Struct, mode should be NO_MODE. */
	/* So mode should not be examined here. */
	/* if (mode == IN_OUT_MODE && who == RP2_CLIENT) { */
    if (who == STUBCLIENT) {
		/* Just copy characters back */
        memcpy(*ptr, buf->buffer, length);
        *ptr[length] = '\0';
    } else {
		/* After the above condition check, the following never occurs.. */
		/* if (mode != NO_MODE && who == RP2_CLIENT) fputc('*', where); */
        *ptr = (RPC2_String)(buf->buffer);
    }
    buf->buffer += _PAD(length);
    return 0;
}


int unpack_countedbs(BUFFER *buf, unsigned char **ptr, RPC2_Unsigned *len_ptr,
        int who)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    if (who == STUBSERVER) {
		/* Special hack */
        *len_ptr = ntohl(*(RPC2_Integer *)(buf->buffer));
        (buf->buffer) += 4;
        if (buf->buffer + _PAD(*len_ptr) > buf->eob)
            return -1;
        *ptr = (RPC2_Byte *)(buf->buffer);
        buf->buffer += _PAD(*len_ptr);
        return 0;
    } else {
        *len_ptr = ntohl(*(RPC2_Integer*)(buf->buffer));
        buf->buffer += 4;
        if (buf->buffer + _PAD(*len_ptr) > buf->eob)
            return -1;
		/*    bug fix. Should update SeqLen and use select. M.K. */
		/*   fprintf(where, "
		    memcpy((char *)%s->SeqBody, %s, (int32_t)%s);\n", */
        memcpy(*ptr, buf->buffer, *len_ptr);
		/*				inc(ptr, length, where); */
        buf->buffer += _PAD(*len_ptr);
        return 0;
    }
}


int unpack_boundedbs(BUFFER *buf, unsigned char **ptr, RPC2_Unsigned *len_ptr,
        RPC2_Unsigned *max_len_ptr, int who, int mode)
{
    if (buf->buffer + 8 > buf->eob)
        return -1;
    if (who == STUBSERVER && mode != STUBIN) {
        *max_len_ptr = ntohl(*(RPC2_Unsigned *)(buf->buffer));
    }
    buf->buffer += 4; /* Skip maximum length */
    if ((who == STUBCLIENT && mode != STUBIN) ||
            (who == STUBSERVER && mode != STUBOUT)) {
        *len_ptr = ntohl(*(RPC2_Unsigned *)(buf->buffer));
    } else if (who == STUBSERVER)
        *len_ptr = 0;
    buf->buffer += 4; /* skip packed sequence length */
    if (who == STUBSERVER && mode == STUBIN)
        *max_len_ptr = *len_ptr;
    else {
        if (*len_ptr > *max_len_ptr)
            return -1;
    }

    if (buf->buffer + _PAD(*len_ptr) > buf->eob)
        return -1;
    if (who == STUBCLIENT) {
        if (mode != STUBIN) {
            memcpy((*ptr), buf->buffer, *len_ptr);
            buf->buffer += _PAD(*len_ptr);
        }
    } else {
        if (*max_len_ptr != 0) {
            *ptr = (RPC2_String)malloc(*max_len_ptr);
            if (*ptr == NULL)
                return -1;
            memcpy((*ptr), buf->buffer, *len_ptr);
            buf->buffer += _PAD(*len_ptr);
        } else {
            *ptr = NULL;
        }
    }
    return 0;

}


int unpack_encryptionKey(BUFFER *buf, char *ptr)
{
    if (buf->buffer + _PAD(RPC2_KEYSIZE) > buf->eob)
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
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Byte *)(buf->buffer) = value;
    buf->buffer += 4;
    return 0;
}


int pack_string(BUFFER *buf, char *ptr)
{
    int length = strlen(ptr);
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Integer *)(buf->buffer) = length;
    if (buf->buffer + 4 + _PAD(length + 1) > buf->eob)
        return -1;
    strcpy(buf->buffer + 4, ptr);
    *(buf->buffer + 4 + length) = '\0';
    buf->buffer += 4 + _PAD(length + 1);
    return 0;
}


int pack_countedbs(BUFFER *buf, char *ptr, RPC2_Unsigned len)
{
    if (buf->buffer + 4 > buf->eob)
        return -1;
    *(RPC2_Unsigned *)(buf->buffer) = htonl(len);
    buf->buffer += 4;
    if (buf->buffer + _PAD(len) > buf->eob)
        return -1;
    memcpy(buf->buffer, ptr, len);
    buf->buffer += _PAD(len);
    return 0;
}


int pack_boundedbs(BUFFER *buf, char *ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len)
{
    if (buf->buffer + 8 + _PAD(len) > buf->eob)
        return -1;
    *(RPC2_Unsigned *)(buf->buffer) = htonl(maxLen);
    if (len == 0)
        *(RPC2_Unsigned *)(buf->buffer + 4) = 0;
    else {
      *(RPC2_Unsigned *)(buf->buffer + 4) = htonl(len);
      memcpy(buf->buffer + 8, ptr, len);
    }
    buf->buffer += 8 + _PAD(len);
    return 0;
}


int pack_encryptionKey(BUFFER *buf, char *ptr)
{
    if (buf->buffer + _PAD(RPC2_KEYSIZE) > buf->eob)
        return -1;
    memcpy(buf->buffer, ptr, RPC2_KEYSIZE);
    buf->buffer += _PAD(RPC2_KEYSIZE);
    return 0;
}


int pack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr, RPC2_Integer who)
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


int unpack_struct_CallCountEntry(BUFFER *buf, CallCountEntry *ptr, RPC2_Integer who)
{
    if (unpack_string(buf, &(ptr->name), 0))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->countent)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->countexit)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tsec)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tusec)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->counttime)))
        return -1;
    return 0;
}


int pack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr, RPC2_Integer who)
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


int unpack_struct_MultiCallEntry(BUFFER *buf, MultiCallEntry *ptr, RPC2_Integer who)
{
    if (unpack_string(buf, &(ptr->name), 0))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->countent)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->countexit)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tsec)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tusec)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->counttime)))
        return -1;
    return 0;
}


int pack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr, RPC2_Integer who)
{
    if (pack_int(buf, ptr->opengate))
        return -1;
    if (pack_int(buf, ptr->tsec))
        return -1;
    if (pack_int(buf, ptr->tusec))
        return -1;
    return 0;
}


int unpack_struct_MultiStubWork(BUFFER *buf, MultiStubWork *ptr, RPC2_Integer who)
{
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->opengate)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tsec)))
        return -1;
    if (unpack_int(buf, (RPC2_Integer *)&(ptr->tusec)))
        return -1;
    return 0;
}

