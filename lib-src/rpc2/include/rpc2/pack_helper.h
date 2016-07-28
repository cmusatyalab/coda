/*********************************************************************************
*     File Name           :     pack_helper.h
*     Created By          :     sil@andrew.cmu.edu
*     Creation Date       :     [2016-07-23 14:48]
*     Last Modified       :     [2016-07-29 21:26]
*     Description         :
**********************************************************************************/
#ifndef _PACK_HELPER_
#define _PACK_HELPER_
#include <rpc2/rpc2.h>
#include <stdlib.h>
#include <string.h>

#define PACKUNPACK_OVERFLOW -1
#define PACKUNPACK_SUCCESS 0
#define STUBCLIENT 0
#define STUBSERVER 1
#define STUBIN 1
#define STUBOUT 2
#define _PAD(n)((((n)-1) | 3) + 1)
static inline int unpack_int(RPC2_Integer* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Integer *)(*old_ptr));
    *old_ptr += 4;
    return 0;
}


static inline int unpack_unsigned(RPC2_Unsigned* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Unsigned*)old_ptr);
    *old_ptr += 4;
    return 0;
}

static inline int unpack_double(RPC2_Double* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 8 > EOB)
        return -1;
    *new_ptr = *(RPC2_Double *)old_ptr;
    *old_ptr += 8;
    return 0;
}

static inline int unpack_bound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB, RPC2_Unsigned len) {
    if (*old_ptr + len > EOB)
        return -1;
    memcpy(new_ptr, *old_ptr, len);
    *old_ptr += _PAD(len);
    return 0;
}

static inline int unpack_unbound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = *(*(RPC2_Byte **)old_ptr);
    *old_ptr += 4;
    return 0;
}

static inline int unpack_string(unsigned char** new_ptr, char** old_ptr, char* EOB, int mode) {
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

static inline int unpack_countedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        char* EOB, int mode) {
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
static inline int unpack_boundedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        RPC2_Unsigned* max_len_ptr, char* EOB, int mode1, int mode2) {
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
            memcpy((unsigned char*)(*new_ptr), *old_ptr, *len_ptr);
            *old_ptr += _PAD(*len_ptr);
        }
    } else {
        if (*max_len_ptr != 0) {
            *new_ptr = (RPC2_String)malloc(*max_len_ptr);
            if (*new_ptr == NULL)
                return -1;
            memcpy((unsigned char*)(*new_ptr), *old_ptr, *len_ptr);
            *old_ptr += _PAD(*len_ptr);
        } else {
            *new_ptr = NULL;
        }
    }
    return 0;

}

static inline int unpack_encryptionKey(char* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + _PAD(RPC2_KEYSIZE) > EOB)
        return -1;
    memcpy((char*)new_ptr, *old_ptr, RPC2_KEYSIZE);
    *old_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}

static inline int pack_int(char** new_ptr, RPC2_Integer value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Integer *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


static inline int pack_unsigned(char** new_ptr, RPC2_Unsigned value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Unsigned *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


static inline int pack_double(char** new_ptr, RPC2_Double value, char* EOB) {
    if (*new_ptr + 8 > EOB)
        return -1;
    *(RPC2_Double *)new_ptr = value;
    *new_ptr += 8;
    return 0;
}

static inline int pack_bound_bytes(char** new_ptr, char* old_ptr, char* EOB, long len) {
    if (*new_ptr + len > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, len);
    *new_ptr += _PAD(len);
    return 0;
}

static inline int pack_unbound_bytes(char** new_ptr, RPC2_Byte value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = value;
    *new_ptr += 4;
    return 0;
}

static inline int pack_string(char** new_ptr, char* old_ptr) {
    int length = strlen(old_ptr);
    *(*(RPC2_Integer **)(new_ptr)) = length;
    strcpy(*new_ptr + 4, old_ptr);
    *(*new_ptr + 4 + length) = '\0';
    *new_ptr += 4 + _PAD(length + 1);
    return 0;
}

static inline int pack_countedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned len) {
    *(*(RPC2_Unsigned **)new_ptr) = htonl(len);
    memcpy(*new_ptr + 4, old_ptr, len);
    *new_ptr += 4 + _PAD(len);
    return 0;
}

static inline int pack_boundedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len) {
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

static inline int pack_encryptionKey(char** new_ptr, char* old_ptr, char* EOB) {
    if (*new_ptr + RPC2_KEYSIZE > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, RPC2_KEYSIZE);
    *new_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}

#endif



