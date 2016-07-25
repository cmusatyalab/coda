/*********************************************************************************
*     File Name           :     pack_helper.h
*     Created By          :     sil@andrew.cmu.edu
*     Creation Date       :     [2016-07-23 14:48]
*     Last Modified       :     [2016-07-23 18:10]
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
static void inline unpack_int(char* ptr, RPC2_Integer* value) {
    *value = ntohl(*(RPC2_Integer *)ptr);
}


static void inline unpack_unsigned(char* ptr, RPC2_Unsigned* value) {
    *value = ntohl(*(RPC2_Unsigned*)ptr);
}

static void inline unpack_double(char* ptr, RPC2_Double* value) {
    *value = *(RPC2_Double *)ptr;
}

static void inline unpack_bound_bytes(unsigned char* new_ptr, char* old_ptr, long len) {
    memcpy(new_ptr, old_ptr, len);
}

static void inline unpack_unbound_bytes(unsigned char* new_ptr, RPC2_Byte value) {
    *(RPC2_Byte *)new_ptr = value;
}

static int inline unpack_string(unsigned char** new_ptr, char** old_ptr, char* eob, int mode) {
    RPC2_Integer length = 1 + ntohl(*(RPC2_Integer *)(*old_ptr));
    *old_ptr += 4;
    if (*old_ptr + _PAD(length) > eob)
        return PACKUNPACK_OVERFLOW;
    if (*(*old_ptr + length - 1) != '\0')
        return PACKUNPACK_OVERFLOW;
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
    return PACKUNPACK_SUCCESS;
}

static int inline unpack_countedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        char* eob, int mode) {
    if (mode == STUBSERVER) {
		/* Special hack */
        *len_ptr = ntohl(*(RPC2_Integer*)(*old_ptr));
        *old_ptr += 4;
        if (*old_ptr + _PAD(*len_ptr) > eob)
            return PACKUNPACK_OVERFLOW;
        *new_ptr = (RPC2_Byte *)(*old_ptr);
        *old_ptr += _PAD(*len_ptr);
        return PACKUNPACK_SUCCESS;
    } else {
        *len_ptr = ntohl(*(RPC2_Integer*)(*old_ptr));
        *old_ptr += 4;
        if (*old_ptr + _PAD(*len_ptr) > eob)
            return PACKUNPACK_OVERFLOW;
		/*    bug fix. Should update SeqLen and use select. M.K. */
		/*   fprintf(where, "
		    memcpy((char *)%s->SeqBody, %s, (int32_t)%s);\n", */
        memcpy(*new_ptr, *old_ptr, (long)*len_ptr);
		/*				inc(ptr, length, where); */
        *old_ptr += _PAD(*len_ptr);
        return PACKUNPACK_SUCCESS;
    }
}
static int inline unpack_boundedbs(unsigned char** new_ptr, char** old_ptr, RPC2_Unsigned* len_ptr,
        RPC2_Unsigned* max_len_ptr, char* eob, int mode1, int mode2) {
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
            return PACKUNPACK_OVERFLOW;
    }

    if (*old_ptr + _PAD(*len_ptr) > eob)
            return PACKUNPACK_OVERFLOW;
    if (mode1 == STUBCLIENT) {
        if (mode2 != STUBIN) {
            memcpy((unsigned char*)(*new_ptr), *old_ptr, (long)*len_ptr);
            *old_ptr += _PAD(*len_ptr);
        }
    } else {
        if (*max_len_ptr != 0) {
            *new_ptr = (RPC2_String)malloc(*max_len_ptr);
            if (*new_ptr == NULL)
                return PACKUNPACK_OVERFLOW;
            memcpy((unsigned char*)(*new_ptr), *old_ptr, (long)*len_ptr);
            *old_ptr += _PAD(*len_ptr);
        } else {
            *new_ptr = NULL;
        }
    }
    return PACKUNPACK_SUCCESS;

}

static void inline unpack_encryptionKey(char* new_ptr, char* old_ptr) {
    memcpy((char*)new_ptr, old_ptr, RPC2_KEYSIZE);
}

static void inline pack_int(char* ptr, RPC2_Integer value) {
    *(RPC2_Integer *)ptr = value;
}


static void inline pack_unsigned(char* ptr, RPC2_Unsigned value) {
    *(RPC2_Unsigned *)ptr = value;
}


static void inline pack_double(char* ptr, RPC2_Double value) {
    *(RPC2_Double *)ptr = value;
}

static void inline pack_bound_bytes(char* new_ptr, char* old_ptr, long len) {
    memcpy(new_ptr, old_ptr, len);
}

static void inline pack_unbound_bytes(char* new_ptr, RPC2_Byte value) {
    *(RPC2_Byte *)new_ptr = value;
}

static void inline pack_string(char* new_ptr, char* old_ptr) {
    int length = strlen(old_ptr);
    *(RPC2_Integer *)(new_ptr) = length;
    strcpy(new_ptr + 4, old_ptr);
    *(new_ptr + 4 + length) = '\0';
}

static void inline pack_countedbs(char* new_ptr, char* old_ptr, RPC2_Unsigned len) {
    *(RPC2_Integer *)new_ptr = htonl(len);
    memcpy(new_ptr + 4, old_ptr, (long)len);
}

static void inline pack_boundedbs(char* new_ptr, char* old_ptr, RPC2_Unsigned maxLen, RPC2_Unsigned len) {
    *(RPC2_Integer *)new_ptr = htonl(maxLen);
    if (len == 0)
        *(RPC2_Integer *)(new_ptr + 4) = 0;
    else {
      *(RPC2_Integer *)(new_ptr + 4) = htonl(len);
      memcpy(new_ptr + 8, old_ptr, (long)len);
    }
}

static void inline pack_encryptionKey(char* new_ptr, char* old_ptr) {
    memcpy(new_ptr, old_ptr, RPC2_KEYSIZE);
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



