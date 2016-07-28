/*********************************************************************************
*     File Name           :     pack_helper.h
*     Created By          :     sil@andrew.cmu.edu
*     Creation Date       :     [2016-07-23 14:48]
*     Last Modified       :     [2016-07-28 11:06]
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
int unpack_int(RPC2_Integer* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Integer *)(*old_ptr));
    *old_ptr += 4;
    return 0;
}


int unpack_unsigned(RPC2_Unsigned* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *new_ptr = ntohl(*(RPC2_Unsigned*)ptr);
    *old_ptr += 4;
    return 0;
}

int unpack_double(RPC2_Double* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 8 > EOB)
        return -1;
    *new_ptr = *(RPC2_Double *)ptr;
    *old_ptr += 8;
    return 0;
}

int unpack_bound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB, RPC2_Unsigned len) {
    if (*old_ptr + len > EOB)
        return -1;
    memcpy(new_ptr, *old_ptr, len);
    *old_ptr += _PAD(len);
    return 0;
}

int unpack_unbound_bytes(unsigned char* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = *(*(RPC2_Byte **)old_ptr);
    *old_ptr += 4;
    return 0;
}

int unpack_string(unsigned char** new_ptr, char** old_ptr, char* EOB, int mode) {
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
int unpack_boundedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
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

int unpack_encryptionKey(char* new_ptr, char** old_ptr, char* EOB) {
    if (*old_ptr + _PAD(RPC2_KEYSIZE) > EOB)
        return -1;
    memcpy((char*)new_ptr, *old_ptr, RPC2_KEYSIZE);
    *old_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}

int pack_int(char** new_ptr, RPC2_Integer value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Integer *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


int pack_unsigned(char** new_ptr, RPC2_Unsigned value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Unsigned *)new_ptr = htonl(value);
    *new_ptr += 4;
    return 0;
}


int pack_double(char** new_ptr, RPC2_Double value, char* EOB) {
    if (*new_ptr + 8 > EOB)
        return -1;
    *(RPC2_Double *)new_ptr = value;
    *new_ptr += 8;
    return 0;
}

int pack_bound_bytes(char** new_ptr, char* old_ptr, char* EOB, long len) {
    if (*new_ptr + len > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, len);
    *new_ptr += _PAD(len);
    return 0;
}

int pack_unbound_bytes(char** new_ptr, RPC2_Byte value, char* EOB) {
    if (*new_ptr + 4 > EOB)
        return -1;
    *(RPC2_Byte *)new_ptr = value;
    *new_ptr += 4;
    return 0;
}

int pack_string(char** new_ptr, char* old_ptr) {
    int length = strlen(old_ptr);
    *(*(RPC2_Integer **))(new_ptr) = length;
    strcpy(*new_ptr + 4, old_ptr);
    *(*new_ptr + 4 + length) = '\0';
    *new_ptr += 4 + _PAD(length + 1);
    return 0;
}

int pack_countedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned len) {
    *(*(RPC2_Unsigned **))new_ptr = htonl(len);
    memcpy(*new_ptr + 4, old_ptr, len);
    *new_ptr += 4 + _PAD(len);
    return 0;
}

int pack_boundedbs(char** new_ptr, char* old_ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len) {
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

int pack_encryptionKey(char** new_ptr, char* old_ptr, char* EOB) {
    if (*new_ptr + RPC2_KEYSIZE > EOB)
        return -1;
    memcpy(*new_ptr, old_ptr, RPC2_KEYSIZE);
    *new_ptr += _PAD(RPC2_KEYSIZE);
    return 0;
}


#define PACK_STRUCT(num, ...) \
        long _arr##num[] = {__VA_ARGS__}; \
        idx = 0; \
        while (idx <= sizeof(_arr##num)/sizeof(long) - 2) { \
            switch((int)_arr##num[idx]) { \
            case RPC2_INTEGER_TAG: \
                pack_int(_ptr, htonl((RPC2_Integer)_arr##num[idx + 1])); \
               _ptr += 4; \
		idx += 2; \
               break; \
            case RPC2_UNSIGNED_TAG: \
                pack_unsigned(_ptr, htonl((RPC2_Unsigned)_arr##num[idx + 1])); \
               _ptr += 4; \
		idx += 2; \
               break; \
            case RPC2_BYTE_TAG: \
                if (idx + 2 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                if (_arr##num[idx + 2] == 0) { \
                    pack_unbound_bytes(_ptr, (RPC2_Byte)_arr##num[idx + 1]); \
                    _ptr += 4; \
                 } else { \
                    pack_bound_bytes(_ptr, _arr##num[idx + 1] + startPtr, _arr##num[idx + 2]); \
                    _ptr += _PAD((RPC2_Unsigned)_arr##num[idx + 2]); \
                 } \
		idx += 3; \
                break; \
            case RPC2_ENUM_TAG: \
                pack_int(_ptr, htonl((RPC2_Integer)_arr##num[idx + 1])); \
                _ptr += 4; \
                idx += 2; \
                break; \
            case RPC2_DOUBLE_TAG: \
                pack_double(_ptr, (RPC2_Double)_arr##num[idx + 1]); \
                _ptr += 8; \
                idx += 2; \
                break; \
            case RPC2_STRING_TAG: \
                _length = strlen(_arr##num[idx + 1] + startPtr); \
                pack_string(_ptr, _arr##num[idx + 1] + startPtr); \
                _ptr += 4 + _PAD(_length + 1); \
                idx += 2; \
                break; \
            case RPC2_COUNTEDBS_TAG: \
                if (idx + 2 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                pack_countedbs(_ptr, _arr##num[idx + 1] + startPtr, (RPC2_Unsigned)_arr##num[idx + 2]); \
                _ptr += 4 + _PAD((RPC2_Unsigned)_arr##num[idx + 2]); \
                idx += 3; \
                break; \
            case RPC2_BOUNDEDBS_TAG: \
                if (idx + 2 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                if (idx + 3 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                pack_boundedbs(_ptr, _arr##num[idx + 1] + startPtr, (RPC2_Unsigned)_arr##num[idx + 2], (RPC2_Unsigned)_arr##num[idx + 3]); \
                _ptr += 8 + _PAD((RPC2_Unsigned)_arr##num[idx + 3]); \
                idx += 4; \
                break; \
            case RPC2_ENCRYPTIONKEY_TAG: \
                pack_encryptionKey(_ptr, _arr##num[idx + 1] + startPtr); \
                _ptr += _PAD(RPC2_KEYSIZE); \
                idx += 2; \
               break; \
            default: \
                break; \
            } \
        }


#define UNPACK_STRUCT(num, ...) \
        long _arr##num[] = {__VA_ARGS__}; \
	idx = 0; \
        while (idx <= sizeof(_arr##num)/sizeof(long) - 2) { \
            switch((int)_arr##num[idx]) { \
            case RPC2_INTEGER_TAG: \
                if (_ptr + 4 > _EOB) \
                    goto bufferoverflow; \
                unpack_int(_ptr, (RPC2_Integer *)(_arr##num[idx + 1] + startPtr)); \
               _ptr += 4; \
		idx += 2; \
               break; \
            case RPC2_UNSIGNED_TAG: \
                if (_ptr + 4 > _EOB) \
                    goto bufferoverflow; \
                unpack_unsigned(_ptr, (RPC2_Unsigned *)(_arr##num[idx + 1] + startPtr)); \
               _ptr += 4; \
		idx += 2; \
               break; \
            case RPC2_BYTE_TAG: \
                if (idx + 2 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                if (_arr##num[idx + 2] == 0) { \
                    if (_ptr + 4 > _EOB) \
                        goto bufferoverflow; \
                    unpack_unbound_bytes((unsigned char*)(_arr##num[idx + 1] + startPtr), *(RPC2_Byte *)_ptr); \
                    _ptr += 4; \
                 } else { \
                    if (_ptr + _arr##num[idx + 2] > _EOB) \
                        goto bufferoverflow; \
                    unpack_bound_bytes( (unsigned char*)(_arr##num[idx + 1] + startPtr), _ptr, (long)_arr##num[idx + 2]); \
                    _ptr += _PAD((RPC2_Unsigned)_arr##num[idx + 2]); \
                 } \
		idx += 3; \
                break; \
            case RPC2_ENUM_TAG: \
                if (_ptr + 4 > _EOB) \
                    goto bufferoverflow; \
                unpack_int(_ptr, (RPC2_Integer*)(_arr##num[idx + 1] + startPtr)); \
                _ptr += 4; \
                idx += 2; \
                break; \
            case RPC2_DOUBLE_TAG: \
                if (_ptr + 8 > _EOB) \
                    goto bufferoverflow; \
                unpack_double(_ptr, (RPC2_Double*)(_arr##num[idx + 1] + startPtr)); \
                _ptr += 8; \
                idx += 2; \
                break; \
            case RPC2_STRING_TAG: \
                if (_ptr + 4 > _EOB) \
                    goto bufferoverflow; \
                if (unpack_string((unsigned char**)(_arr##num[idx + 1] + &startPtr), &_ptr, _EOB, _arr##num[idx + 2]) < 0) \
                    goto bufferoverflow; \
                idx += 3; \
                break; \
            case RPC2_COUNTEDBS_TAG: \
                if (idx + 2 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                if (_ptr + 4 > _EOB) \
                    goto bufferoverflow; \
                if (unpack_countedbs((unsigned char**)(_arr##num[idx + 1] + &startPtr), &_ptr, (RPC2_Unsigned *)(_arr##num[idx + 2] + startPtr), _EOB, _arr##num[idx + 3]) < 0) \
                    goto bufferoverflow; \
                idx += 4; \
                break; \
            case RPC2_BOUNDEDBS_TAG: \
                if (idx + 5 > sizeof(_arr##num)/sizeof(long)) { \
                    goto bufferoverflow; \
                } \
                if (unpack_boundedbs((unsigned char**)(_arr##num[idx + 1] + &startPtr), &_ptr,  (RPC2_Unsigned *)(_arr##num[idx + 2] + startPtr), (RPC2_Unsigned *)(_arr##num[idx + 3] + startPtr), _EOB,  _arr##num[idx + 4], _arr##num[idx + 5]) < 0) \
                    goto bufferoverflow; \
                idx += 6; \
                break; \
            case RPC2_ENCRYPTIONKEY_TAG: \
                if (_ptr + _PAD(RPC2_KEYSIZE) > _EOB) \
                    goto bufferoverflow; \
                unpack_encryptionKey((char*)(_arr##num[idx + 1] + startPtr), _ptr); \
                _ptr += _PAD(RPC2_KEYSIZE); \
                idx += 2; \
               break; \
            default: \
                break; \
            } \
        }

#endif



