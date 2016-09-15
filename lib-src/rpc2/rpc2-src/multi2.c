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


/*
	Routines for MultiRPC
*/

/* MRPC_MakeMulti() and MRPC_UnpackMulti() perform argument packing
 * and unpacking for the RPC2_MultiRPC() call. This is a library
 * routine which gets its type information from definitions in RP2GEN
 * generated include files and client side interface.  */

#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include <rpc2/multi.h>

#define _ROUND(n, a) (n) = (void *)((((intptr_t)n)+((a)-1)) & ~((a)-1))
#define _INCR(n, a)  (n) = (void *)((intptr_t)(n) + a)

#define _PAD(n)     (((n)+3) & ~3)
#define _PADWORD(n) (((n)+1) & ~1)
#define _PADLONG(n) _PAD(n)

long MRPC_UnpackMulti(int HowMany, RPC2_Handle ConnHandleList[],
                      ARG_INFO *ArgInfo, RPC2_PacketBuffer *rspbuffer,
                      long rpcval, long offset);

int  get_len(ARG **a_types, PARM **args, MODE mode);
void pack(ARG *a_types, PARM **args, unsigned char **_ptr);
void pack_struct(ARG *a_types, PARM **args, unsigned char **ptr);
static unsigned int  get_arraylen_pack(ARG *a_types, PARM *args);
static void incr_struct_byte(ARG *a_types, PARM **args);
static int new_unpack(ARG *a_types, PARM **args, unsigned char **_ptr, char *_end,
	       long offset);
int unpack_struct(ARG *a_types, PARM **args, unsigned char **_ptr, char *_end,
		  long offset);
static void byte_pad(PARM **args);

/*
    ServerOp	RP2Gen generated op code 
    ArgTypes	format of server arguments 
    HowMany	how many multiple servers
    CIDList	list of connection ids
    RCList	NULL or pointer to array for individual return codes
    HandleResult	user procedure to be called after each server return
    Timeout	user specified timeout
*/
long MRPC_MakeMulti (int ServerOp, ARG ArgTypes[], RPC2_Integer HowMany,
		     RPC2_Handle CIDList[], RPC2_Integer RCList[],
		     RPC2_Multicast *MCast,
		     RPC2_HandleResult_func *HandleResult,
		     struct timeval *Timeout,  ...)
{
    RPC2_PacketBuffer *_reqbuffer;
    struct timeval;
    PARM *args;
    PARM *va_array;		/* a copy of those variable-length arguments */
    unsigned char *_ptr;
    ARG *a_types;
    ARG_INFO arg_info;
    SE_Descriptor *SDescList = NULL;
    long _length, _rpc2val;
    int count = 0, i;
    va_list ap;

    /* first we need to know how many arguments in the variable-length
       argument lists */
    for(a_types = ArgTypes, i=0; a_types->mode != C_END; a_types++)
	    i++;
    va_array = malloc((i * sizeof(PARM)) +1); /* and then malloc the storage
						 (add one to avoid malloc(0)
						 when i == 0) */
    assert((va_array!=0));   /* don't know better way to handle "Can't malloc" */
    
    /* the followings are safe and standard way to get those
       variable-length arguments */
    va_start(ap, Timeout);
    for(a_types = ArgTypes, i=0; a_types->mode != C_END; a_types++, i++) {
	    switch(a_types->type) {
	    case RPC2_INTEGER_TAG:  /* 0: begin of case RPC2_INTEGER_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].integer = va_arg(ap, RPC2_Integer);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].integerp = va_arg(ap, RPC2_Integer **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 0: end   of case RPC2_INTEGER_TAG */
	    case RPC2_UNSIGNED_TAG: /* 1: begin of case RPC_UNSIGNED_TAG */
		    switch (a_types->mode) {
		    case IN_MODE:
			    va_array[i].unsgned = va_arg(ap, RPC2_Unsigned);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].unsgnedp = va_arg(ap, RPC2_Unsigned **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 1: end of case RPC2_UNSIGNED_TAG */
	    case RPC2_BYTE_TAG:	/* 2: begin of case RPC2_BYTE_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].byte = (RPC2_Byte)va_arg(ap, int);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].bytep = va_arg(ap, RPC2_Byte **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 2: end   of case RPC2_BYTE_TAG */
	    case RPC2_STRING_TAG:	/* 3: begin of case RPC2_STRING_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].string = va_arg(ap, RPC2_String);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].stringp = va_arg(ap, RPC2_String **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 3: end   of case RPC2_STRING_TAG */
	    case RPC2_COUNTEDBS_TAG:/* 4: begin of case RPC2_COUNTEDBS_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].cbs = va_arg(ap, RPC2_CountedBS *);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].cbsp = va_arg(ap, RPC2_CountedBS **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 4: end   of case RPC2_COUNTEDBS_TAG */
	    case RPC2_BOUNDEDBS_TAG:/* 5: begin of case RPC2_BOUNDEDBS_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].bbs = va_arg(ap, RPC2_BoundedBS *);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].bbsp = va_arg(ap, RPC2_BoundedBS **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 5: end   of case RPC2_BOUNDEDBS_TAG */
	    case RPC2_BULKDESCRIPTOR_TAG: /* 6: begin of case RPC2_BULKDESCRIPTOR_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
		    case OUT_MODE:
		    case IN_OUT_MODE:	/* not sure if this is correct way: bulk descriptor is
					   not documented in Ch.2 of RPC2 manual*/
			    va_array[i].sedp = va_arg(ap, SE_Descriptor *);
			    break;
		    default:
			    assert(0);
		    }
		    break;                    /* 6: end   of case RPC2_BULKDESCRIPTOR_TAG */
	    case RPC2_ENCRYPTIONKEY_TAG:  /* 7: begin of case RPC2_ENCRYPTIONKEY_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].key = va_arg(ap, RPC2_EncryptionKey *);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].keyp = va_arg(ap, RPC2_EncryptionKey **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;                    /* 7: end   of case RPC2_ENCRYPTIONKEY_TAG */
	    case RPC2_STRUCT_TAG:	/* 8: begin of case RPC2_STRUCT_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	
			    va_array[i].structp = va_arg(ap, union PARM *);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].structpp = va_arg(ap, union PARM **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 8: end   of case RPC2_STRUCT_TAG */
	    case RPC2_ENUM_TAG:	/* 9: begin of case RPC2_ENUM_TAG */
		    switch(a_types->mode) {
		    case IN_MODE:	/* is this the right way to ENUM parameter ? */
			    va_array[i].integer = va_arg(ap, RPC2_Integer);
			    break;
		    case OUT_MODE:
		    case IN_OUT_MODE:
			    va_array[i].integerp = va_arg(ap, RPC2_Integer **);
			    break;
		    default:
			    assert(0);      
		    }
		    break;              /* 9: end   of case RPC2_ENUM_TAG */
	    default:
		    assert(0);
	    } /* end of switch on a_types */
    } /* end of stepping thru the list of variable-length arguments */
    va_end(ap);
    
    _length = 0;
    
    count = 0;
    for(a_types = ArgTypes, args = va_array; a_types->mode != C_END ; 
	a_types++, args++, count++) {
	    switch(a_types->mode){
	    case OUT_MODE:
		    if (a_types->type == RPC2_BOUNDEDBS_TAG)
			_length += get_len(&a_types, &args, a_types->mode);
		    break;
	    case C_END:
		    break;
	    case IN_MODE:
	    case IN_OUT_MODE:
		    switch(a_types->type) {
		    case RPC2_STRUCT_TAG:
			    _length += struct_len(&a_types, &args);
			    break;

		    case RPC2_BULKDESCRIPTOR_TAG:
			    a_types->bound = 0;
			    SDescList = args[0].sedp;
			    break;

		    default:
			    a_types->bound = 0;
			    _length += get_len(&a_types, &args, a_types->mode);

		    }
		    break;

	    case NO_MODE:
		    say(0, RPC2_DebugLevel, "MRPC_MakeMulti: bad mode for argument NO_MODE\n");
	    }
    }
    
    for(a_types = ArgTypes; ; a_types++)
	    if (a_types->mode == C_END) {
		    (*a_types->startlog)(ServerOp); /* Call Stub log start */
		    break;
	    }
    
    _rpc2val = RPC2_AllocBuffer(_length, &_reqbuffer);
    if (_rpc2val != RPC2_SUCCESS) {
	free(va_array);
	return _rpc2val;
    }

    /* Pack arguments */
    _ptr = _reqbuffer->Body;
    for(a_types = ArgTypes, args = va_array; a_types->mode != C_END; a_types++) {
	    if (a_types->mode != OUT_MODE || a_types->type == RPC2_BOUNDEDBS_TAG) {
		    if (a_types->type != RPC2_STRUCT_TAG)
			    pack(a_types, &args, &_ptr);
		    else {
			    pack_struct(a_types, &args, &_ptr);
			    args++;
		    }
	    }
	    else args++;
    }

    /* Pack server argument info into structure */
    arg_info.ArgTypes = ArgTypes;
    arg_info.Args = va_array;
    arg_info.HandleResult = HandleResult;
    arg_info.ArgCount = count;

    /* Generate RPC2 call */

    _reqbuffer->Header.Opcode = ServerOp;

    _rpc2val = RPC2_MultiRPC(HowMany, CIDList, RCList, NULL, _reqbuffer, SDescList, MRPC_UnpackMulti, &arg_info, Timeout);

    for(a_types = ArgTypes; ; a_types++)
	    if (a_types->mode == C_END) {
		    (*a_types->endlog)(ServerOp, HowMany, CIDList, RCList); /* Call Stub Log end */
		    break;
	    }

    free(va_array);		/* done with the array */
    if (_rpc2val != RPC2_SUCCESS) {
	    RPC2_FreeBuffer (&_reqbuffer);
	    
	    return _rpc2val;
    }
    _rpc2val = RPC2_FreeBuffer(&_reqbuffer);
    return _rpc2val;
}

/* Packs the given structure into the RequestBuffer (called recursively) */
void pack_struct(ARG *a_types, PARM **args, unsigned char **ptr)
{
	ARG *field;
	PARM **strp, *str;
	int i, maxiterate = 1;

	if (a_types->mode == IN_OUT_MODE) {
		str = *(*args)->structpp;
		strp = &str;
	}
	else if (a_types->mode == IN_MODE) {
		str = (*args)->structp;
		strp = &str;
	}
	else strp = args;

	if (a_types->bound != 0)
	    maxiterate = get_arraylen_pack(a_types-1, *args-1);

	for(i = 0; i < maxiterate; i++) {
	    for(field = a_types->field; field->mode != C_END; field++) {
		if (field->type == RPC2_STRUCT_TAG)
		    pack_struct(field, strp, ptr);
		else pack(field, strp, ptr);
	    }
	}
}

/* Packs the given type into the RequestBuffer */
void pack(ARG *a_types, PARM **args, unsigned char **_ptr)
{
    unsigned char *_body;
    int32_t _length, _maxlength;
    MODE mode = a_types->mode;

    switch(a_types->type) {
	case RPC2_UNSIGNED_TAG:
	    if (mode == IN_OUT_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		*(uint32_t *)(*_ptr) = htonl(**(*args)->unsgnedp);
		_INCR(*args, sizeof(intptr_t));
	    } else {
		_ROUND(*args, sizeof(uint32_t));
		*(uint32_t *)(*_ptr) = htonl((*args)->unsgned);
		_INCR(*args, sizeof(uint32_t));
	    }

	    _INCR(*_ptr, sizeof(uint32_t));
	    break;

	case RPC2_INTEGER_TAG:
	case RPC2_ENUM_TAG:
	    if (mode == IN_OUT_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		*(int32_t *)(*_ptr) = htonl(**(*args)->integerp);
		_INCR(*args, sizeof(intptr_t));
	    } else {
		_ROUND(*args, sizeof(int32_t));
		*(int32_t *)(*_ptr) = htonl((*args)->integer);
		_INCR(*args, sizeof(int32_t));
	    }

	    _INCR(*_ptr, sizeof(int32_t));
	    break;

	case RPC2_BYTE_TAG:
	    if (a_types->bound != 0) {	/* Byte array */
		if (mode == IN_MODE)
		{
		    _ROUND(*args, sizeof(intptr_t));
		    memcpy(*_ptr, (*args)->bytep, a_types->bound);
		    _INCR(*args, sizeof(intptr_t));
		}
		else if(mode == IN_OUT_MODE)
		{
		    _ROUND(*args, sizeof(intptr_t));
		    memcpy(*_ptr, *(*args)->bytep, a_types->bound);
		    _INCR(*args, sizeof(intptr_t));
		}
		else if (mode == NO_MODE) /* structure field */
		{
		    memcpy(*_ptr, &(*args)->byte, a_types->bound);
		    incr_struct_byte(a_types, args);
		}
		_INCR(*_ptr, a_types->size);
		_ROUND(*_ptr, sizeof(int32_t));
	    }
	    else {			/* single byte */
		if (mode == IN_OUT_MODE) {
		    _ROUND(*args, sizeof(intptr_t));
		    /* **_ptr = **(*args)->integerp; */ /* bug fix */
		    **_ptr = **((*args)->bytep);
		    _INCR(*args, sizeof(intptr_t));
		}
		else if (mode == IN_MODE) /* IN mode only */
		{
		    **_ptr = (*args)->integer;
		    _INCR(*args, sizeof(int8_t));
		}
		else if (mode == NO_MODE) /* structure field */
		{
		    **_ptr = *(unsigned char *)(*args);
		    incr_struct_byte(a_types, args);
		}
		_INCR(*_ptr, sizeof(uint32_t));
	    }
	    break;

	case RPC2_STRING_TAG:
	    _body = (mode == IN_OUT_MODE) ? **(*args)->stringp :(*args)->string;

	    _length = strlen((char *)_body);
	    *(int32_t *)(*_ptr) = htonl(_length);
	    _INCR(*_ptr, sizeof(int32_t));

	    (void) strcpy((char *)*_ptr, (char *)_body);
	    (*_ptr)[_length] = '\0';
	    _INCR(*_ptr, _length+1);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_COUNTEDBS_TAG:
	    if (mode == NO_MODE) {
		_ROUND(*args, sizeof(int32_t));
		_length = ((RPC2_CountedBS *)(*args))->SeqLen;
		_body = ((RPC2_CountedBS *)(*args))->SeqBody;

		_INCR(*args, sizeof(int32_t));
	    }
	    else if (mode == IN_OUT_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		_length = (*(*args)->cbsp)->SeqLen;
		_body = (*(*args)->cbsp)->SeqBody;
	    }
	    else {
		_ROUND(*args, sizeof(intptr_t));
		_length = (*args)->cbs->SeqLen;
		_body = (*args)->cbs->SeqBody;
	    }
	    *(int32_t *)(*_ptr) = htonl(_length);
	    _INCR(*_ptr, sizeof(int32_t));
	    memcpy(*_ptr, _body, _length);
	    _INCR(*_ptr, _length);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_BOUNDEDBS_TAG:
	    if (mode == NO_MODE) {
		_ROUND(*args, sizeof(int32_t));
		_maxlength = ((RPC2_BoundedBS *)(*args))->MaxSeqLen;
		_length = ((RPC2_BoundedBS *)(*args))->SeqLen;
		_body = ((RPC2_BoundedBS *)(*args))->SeqBody;
		_INCR(*args, 2 * sizeof(int32_t));
	    }
	    else if (mode == IN_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		/* pack an 'unused' MaxSeqLen */
		_maxlength = (*(*args)->bbs).SeqLen;
		_length = (*(*args)->bbs).SeqLen;
		_body = (*(*args)->bbs).SeqBody;
	    }
	    else if (mode == IN_OUT_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		_maxlength = (*(*args)->bbsp)->MaxSeqLen;
		_length = (*(*args)->bbsp)->SeqLen;
		_body = (*(*args)->bbsp)->SeqBody;
	    }
	    else { /* OUT_MODE */
		_ROUND(*args, sizeof(intptr_t));
		_maxlength = (*(*args)->bbsp)->MaxSeqLen;
		_length = 0;
		_body = NULL;
	    }

	    *(int32_t *)(*_ptr) = htonl(_maxlength);
	    _INCR(*_ptr, sizeof(int32_t));
	    *(int32_t *)(*_ptr) = htonl(_length);
	    _INCR(*_ptr, sizeof(int32_t));
	    if (_length) {
		memcpy(*_ptr, (char *)_body, _length);
		_INCR(*_ptr, _length);
		_ROUND(*_ptr, sizeof(int32_t));
	    }

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_ENCRYPTIONKEY_TAG:
	    if (mode == IN_OUT_MODE) {
		_ROUND(*args, sizeof(intptr_t));
		memcpy(*_ptr, (*(*args)->keyp[0]), RPC2_KEYSIZE);
	    }
	    else {
		memcpy(*_ptr, *(*args)->key, RPC2_KEYSIZE);
	    }
	    _INCR(*_ptr, RPC2_KEYSIZE);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_BULKDESCRIPTOR_TAG:
	    break;

	case RPC2_STRUCT_TAG:
	    say(0, RPC2_DebugLevel, "MakeMulti (pack): RPC2_STRUCT_TAG encountered\n");
	    break;

	default:
	    say(0, RPC2_DebugLevel, "MakeMulti (pack): unknown type tag: %d\n", a_types->type);
    }

    if (mode != NO_MODE)
	_ROUND(*args, sizeof(PARM));
}

/* Returns the buffer length needed for the given structure (called
   recursively) */
int struct_len(ARG **a_types, PARM **args)
{
	ARG *field;
	PARM **strp, *str;
	int len = 0;
	int i, maxiterate;

	if ((*a_types)->mode == IN_OUT_MODE) {
		str = *(*args)->structpp;
		strp = &str;
	}
	else if ((*a_types)->mode == IN_MODE) {
		str = (*args)->structp;
		strp = &str;
        }
	else 
		strp = args;
	
	if ((*a_types)->bound != 0) {
		/* Array size should be stored before array structures */
		maxiterate = get_arraylen_pack(*a_types-1, *args-1);
		for(i = 0; i < maxiterate; i++) {
			for(field = (*a_types)->field; field->mode != C_END; field++) {
				if (field->type == RPC2_STRUCT_TAG)
					len += struct_len(&field, strp);
				else len += get_len(&field, strp, NO_MODE);
				switch (field->type) {
				case RPC2_BOUNDEDBS_TAG:
					(*strp)++;
				case RPC2_COUNTEDBS_TAG:
					(*strp)++;
				default:
					(*strp)++;
					break;
				}
			}
		}
	} else {
		for(field = (*a_types)->field; field->mode != C_END; field++) {
			if (field->type == RPC2_STRUCT_TAG)
				len += struct_len(&field, strp);
			else len += get_len(&field, strp, NO_MODE);
			switch (field->type) {
			case RPC2_BOUNDEDBS_TAG:
				(*strp)++;
			case RPC2_COUNTEDBS_TAG:
				(*strp)++;
			default:
				(*strp)++;
				break;
			}
		}
	}
	
	return(len);
}



/* This is the counterpart to MRPC_MakeMulti. It is a separate
 * procedure because it is necessary to unpack arguments multiple
 * times for each call, once for each server response
 * received. MPRC_UnpackMulti and its associated procedures unpack the
 * RPC2 response buffer and call the client handler.  */

/*
 * HowMany		How many servers
 * ConnHandleList	list of connection ids
 * ArgInfo	        server argument values and info
 * rspbuffer    	rpc response buffer
 * rpcval		server return value
 * offset		array index
 */
long MRPC_UnpackMulti(int HowMany, RPC2_Handle ConnHandleList[],
                      ARG_INFO *ArgInfo, RPC2_PacketBuffer *rspbuffer,
                      long rpcval, long offset)
{
    ARG *a_types;	/* holds ArgTypes */
    PARM *args;	/* holds Args */
    PARM *str;
    unsigned char *_ptr;	/* holds rspbuffer */
    int ret = 0;
    char *_end;

    if (rpcval == RPC2_SUCCESS) {
       if(rspbuffer->Header.ReturnCode == RPC2_INVALIDOPCODE)
	  rpcval = RPC2_INVALIDOPCODE;
       else {
	  _ptr = rspbuffer->Body;
	  _end = (char *)_ptr + rspbuffer->Header.BodyLength;
	  rpcval = rspbuffer->Header.ReturnCode;
	  for(a_types = ArgInfo->ArgTypes, args = ArgInfo->Args; a_types->mode != C_END; a_types++) {
	    switch(a_types->mode){
		case IN_MODE:
		    args++;
		    break;
		case OUT_MODE:
		case IN_OUT_MODE:
		    if (a_types->type == RPC2_STRUCT_TAG) {
			str = (PARM *) args->structpp[offset];
			ret = unpack_struct(a_types, &str, &_ptr, _end, offset);
			args++;
		    }
		    else ret = new_unpack(a_types, &args, &_ptr, _end, offset);
		    break;
		default:	assert(FALSE);
	    }
	    if (ret) break;
	  }
       }
    }

   /* Call client routine with arguments and RPC2 return code */
    args = ArgInfo->Args;
    if (ret == 0 && ArgInfo->HandleResult)
	ret = mkcall(ArgInfo->HandleResult, ArgInfo->ArgCount, HowMany,
		     ConnHandleList, offset, rpcval, (int *)args);

    if (rspbuffer != NULL)
	RPC2_FreeBuffer(&rspbuffer);

    return(ret);
}


/* Returns the buffer length needed for the given argument, or -1 if unknown
 * type. Note that this routine modifies the static array of argument type
 * descriptors defined in <subsys>.client.c by changing the value of the 'size'
 * field. 
 */

int get_len(ARG **a_types, PARM **args, MODE mode)
{
    RPC2_CountedBS *cbsbodyp;
    RPC2_BoundedBS *bbsbodyp;
     switch ((*a_types)->type) {
	case RPC2_BYTE_TAG:
			if ((*a_types)->size != 0) {
			   (*a_types)->bound = ((*a_types)->size);
			   return((*a_types)->size = _PAD((*a_types)->bound));
			}
			else return(sizeof(int32_t));	/* don't set (*a_types)->size for single char variable */
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
	case RPC2_ENUM_TAG:
			return ((*a_types)->size);
	case RPC2_STRING_TAG:
			(*a_types)->size = sizeof(int32_t);
	     		if (mode == IN_OUT_MODE)
			    (*a_types)->size += _PAD(strlen((char *)(*(*args)->stringp[0]))+1);
			else
			    (*a_types)->size += _PAD(strlen((char *)(*args)->string)+1);
			return (*a_types)->size;

	case RPC2_COUNTEDBS_TAG:
			(*a_types)->size = sizeof(int32_t);
			if (mode == NO_MODE) {
			    cbsbodyp = (RPC2_CountedBS *)(*args);
			    (*a_types)->size += _PAD(cbsbodyp->SeqLen);
			}
			else if (mode == IN_OUT_MODE)
			    (*a_types)->size += _PAD((*(*args)->cbsp)->SeqLen);

			else
			    (*a_types)->size += _PAD((*args)->cbs->SeqLen);

			return (*a_types)->size;

	case RPC2_BOUNDEDBS_TAG:
			(*a_types)->size = 2 * sizeof(int32_t);
			if (mode == NO_MODE) {
			  bbsbodyp = (RPC2_BoundedBS *)(*args);
			  (*a_types)->size += _PAD(bbsbodyp->SeqLen);
			}
			else if (mode == IN_OUT_MODE)
			  (*a_types)->size += _PAD((*(*args)->bbsp)->SeqLen);

			else if (mode == IN_MODE)
			  (*a_types)->size += _PAD((*(*args)->bbs).SeqLen);

			/* else OUT_MODE */

			return (*a_types)->size;

	case RPC2_BULKDESCRIPTOR_TAG:
	case RPC2_ENCRYPTIONKEY_TAG:
			return((*a_types)->size);
	case RPC2_STRUCT_TAG:		
			say(0, RPC2_DebugLevel, "get_len: struct_tag encountered\n");
			return(-1);
	default:
			say(0, RPC2_DebugLevel, "get_len: [can't happen]: impossible type tag: %d\n", (*a_types)->type);
			return(-1);
     }
}

/* Returns an array size. It is assumed that an array size of an array is declared 
 * in front of array declaration.
 */
static unsigned int get_arraylen_pack(ARG *a_types, PARM *args)
{
    int arraysize;
    switch(a_types->type) {
        case RPC2_UNSIGNED_TAG:
                        if (a_types->mode == IN_OUT_MODE)
			    arraysize = **args->unsgnedp;
			else
			    arraysize = args->unsgned;
			return arraysize;
			/*NOTREACHED*/
			break;
        default:
			say(0, RPC2_DebugLevel, "MakeMulti: cannot pack array size\n");
			exit(-1);
    }
    /*NOTREACHED*/
}

static unsigned int get_arraylen_unpack(ARG *a_types, unsigned char *ptr)
{
    switch(a_types->type) {
        case RPC2_UNSIGNED_TAG:
			return ntohl(*(uint32_t *)ptr);
			/*NOTREACHED*/
			break;
        default:
			say(0, RPC2_DebugLevel, "MakeMulti: cannot unpack array size\n");
			exit(-1);
    }
    /*NOTREACHED*/
}

#define CHECK(size) do { \
    if (((char *)*_ptr + (size)) > _end) \
	return EINVAL; \
    } while(0)

/* buggy but needed, codasrv calls this function directly to unpack the CML */
int unpack(ARG *a_types, PARM *args, unsigned char **_ptr, char *_end, long offset)
{
    return new_unpack(a_types, &args, _ptr, _end, offset);
}

static int new_unpack(ARG *a_types, PARM **args, unsigned char **_ptr, char *_end, long offset)
{
    int32_t _length, _maxlength;
    RPC2_BoundedBS *bbsbodyp;
    MODE mode = a_types->mode;

    switch(a_types->type) {
	case RPC2_UNSIGNED_TAG:
	    CHECK(sizeof(uint32_t));
	    if (mode != NO_MODE) {
		*((*args)->unsgnedp[offset]) = ntohl(*(uint32_t *)(*_ptr));
		_INCR(*args, sizeof(intptr_t));
	    }
	    else {
		(*args)->unsgned = ntohl(*(uint32_t *)(*_ptr));
		_INCR(*args, sizeof(uint32_t));
	    }
	    _INCR(*_ptr, sizeof(uint32_t));
	    break;

	case RPC2_INTEGER_TAG:
	case RPC2_ENUM_TAG:
	    CHECK(sizeof(int32_t));
	    if (mode != NO_MODE) {
		*((*args)->integerp[offset]) = ntohl(*(int32_t *)(*_ptr));
		_INCR(*args, sizeof(intptr_t));
	    }
	    else {
		(*args)->integer = ntohl(*(int32_t *)(*_ptr));
		_INCR(*args, sizeof(uint32_t));
	    }
	    _INCR(*_ptr, sizeof(int32_t));
	    break;

	case RPC2_BYTE_TAG:
	    if (a_types->bound != 0) {
		CHECK(a_types->bound);
		if (mode == NO_MODE) {
		    memcpy(&((*args)->byte), *_ptr, a_types->bound);
		    incr_struct_byte(a_types, args);
		}
		else {
		    memcpy((*args)->bytep[offset], *_ptr, a_types->bound);
		    _INCR(*args, sizeof(intptr_t));
		}
		_INCR(*_ptr, a_types->size);
		_ROUND(*_ptr, sizeof(int32_t));
	    }
	    else {
		CHECK(1);
		if (mode != NO_MODE) {
		    *((*args)->bytep[offset]) = *(RPC2_Byte *)(*_ptr);
		    _INCR(*args, sizeof(intptr_t));
		}
		else {
		    (*args)->byte = *(RPC2_Byte *)(*_ptr);
		    incr_struct_byte(a_types, args);
		}
		_INCR(*_ptr, sizeof(int32_t));
	    }
	    break;

	case RPC2_STRING_TAG:
	    CHECK(sizeof(int32_t));
	    _length = ntohl(*(int32_t *)(*_ptr)) + 1;
	    _INCR(*_ptr, sizeof(int32_t));
	    CHECK(_length);
	    if (mode != NO_MODE) {
		memcpy(*((*args)->stringp[offset]), *_ptr, _length);
		(*(*args)->stringp[offset])[_length - 1] = '\0';
	    }
	    else {
		memcpy((*args)->string, *_ptr, _length);
		(*args)->string[_length - 1] = '\0';  /* used to be [length] */
	    }
	    _INCR(*_ptr, _length);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_COUNTEDBS_TAG:
	    CHECK(sizeof(int32_t));
	    _length = ntohl(*(int32_t *)(*_ptr));
	    _INCR(*_ptr, sizeof(int32_t));
	    CHECK(_length);
	    if (mode != NO_MODE) {
		(*args)->cbsp[offset]->SeqLen = _length;
		memcpy((*args)->cbsp[offset]->SeqBody, *_ptr, _length);
	    }
	    else {
		((RPC2_CountedBS *)(*args))->SeqLen = _length;
		memcpy(((RPC2_CountedBS *)(*args))->SeqBody, *_ptr, _length);
		_INCR(*args, sizeof(int32_t));
	    }
	    _INCR(*_ptr, _length);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_BOUNDEDBS_TAG:
	    CHECK(2*sizeof(int32_t));
	    _maxlength = ntohl(*(int32_t *)(*_ptr));
	    _INCR(*_ptr, sizeof(int32_t));
	    _length = ntohl(*(int32_t *)(*_ptr));
	    _INCR(*_ptr, sizeof(int32_t));
	    CHECK(_length);
	    if (mode == OUT_MODE || mode == IN_OUT_MODE) {
		/* ignore received MaxSeqLen */
		(*args)->bbsp[offset]->SeqLen = _length;
		if (_length <= (*args)->bbsp[offset]->MaxSeqLen)
		    memcpy((*args)->bbsp[offset]->SeqBody, *_ptr, _length);
	    }
	    else if (mode == NO_MODE) {
		bbsbodyp = (RPC2_BoundedBS *)(*args);
		bbsbodyp->MaxSeqLen = _maxlength;
		bbsbodyp->SeqLen = _length;
		memcpy(bbsbodyp->SeqBody, *_ptr, _length);
		_INCR(*args, 2 * sizeof(int32_t));
	    }
	    _INCR(*_ptr, _length);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_ENCRYPTIONKEY_TAG:
	    CHECK(RPC2_KEYSIZE);
	    if (mode == IN_OUT_MODE) {
		memcpy((*args)->keyp[offset], *_ptr, RPC2_KEYSIZE);
	    }
	    else memcpy(*((*args)->key), *_ptr, RPC2_KEYSIZE);
	    _INCR(*_ptr, RPC2_KEYSIZE);
	    _ROUND(*_ptr, sizeof(int32_t));

	    _INCR(*args, sizeof(intptr_t));
	    break;

	case RPC2_BULKDESCRIPTOR_TAG:
	    break;

	case RPC2_STRUCT_TAG:
	    say(0, RPC2_DebugLevel, "Unpack: encountered struct\n");
	    break;

	default:
	    say(0, RPC2_DebugLevel, "UnpackMulti (unpack): unknown tag: %d\n", a_types->type);
    }

    if (mode != NO_MODE)
	_ROUND(*args, sizeof(PARM));
    return 0;
}


int unpack_struct(ARG *a_types, PARM **args, unsigned char **_ptr, char *_end,
		  long offset)
{
    ARG *field;
    PARM **strp, *str;
    int i, maxiterate = 1, ret;

    if (a_types->mode != NO_MODE) {
	str = *args;
	strp = &str;
    }
    else strp = args;

    if (a_types->bound != 0)
        /* Array size should be stored before array structures */
        maxiterate = get_arraylen_unpack(a_types-1, *_ptr-1);

    for(i = 0; i < maxiterate; i++) {
	for(field = a_types->field; field->mode != C_END; field++) {
	    if (field->type == RPC2_STRUCT_TAG)
		ret = unpack_struct(field, strp, _ptr, _end, -1);
	    else
		ret = new_unpack(field, strp, _ptr, _end, offset);
	    if (ret) return ret;
	}
    }
    return 0;
}

/* This should only be called for structure fields, never for top level arguments */
static void incr_struct_byte(ARG *a_types, PARM **args)
  {
	*(char **)args += (a_types->bound) ? (a_types->bound) : 1;
	if (a_types[1].type == RPC2_BYTE_TAG) return;
	byte_pad(args);
  }


static void byte_pad(PARM **args)
{
#ifdef sun
    *(char **)args = (char *)_PADWORD((long) *args);
#endif
#if defined(ibm032) || defined(vax) || defined(mips)
    *(char **)args = (char *)_PADLONG((long) *args);
#endif
}

