/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
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
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include "rpc2.private.h"
#include <rpc2/se.h>
#include <rpc2/multi.h>


#define SIZE 4
#define _PAD(n)(((n)+3) & ~3)
#define _PADWORD(n)(((n)+1) & ~1)
#define _PADLONG(n)_PAD(n)

extern int mkcall(RPC2_HandleResult_func *ClientHandler, int ArgCount,
		  int HowMany, RPC2_Handle ConnList[], long offset, long rpcval,
		  int *args);

long MRPC_UnpackMulti(int HowMany, RPC2_Handle ConnHandleList[],
                      ARG_INFO *ArgInfo, RPC2_PacketBuffer *rspbuffer,
                      long rpcval, long offset);

int  get_len(ARG **a_types, PARM **args, MODE mode);
void pack(ARG *a_types, PARM **args, PARM **_ptr);
void pack_struct(ARG *a_types, PARM **args, PARM **ptr);
int  get_arraylen_pack(ARG *a_types, PARM *args);
void incr_struct_byte(ARG *a_types, PARM **args);
void unpack(ARG *a_types, PARM *args, PARM **_ptr, long offset);
void unpack_struct(ARG *a_types, PARM **args, PARM **_ptr, long offset);
void byte_pad(PARM **args);

/*
    ServerOp	RP2Gen generated op code 
    ArgTypes	format of server arguments 
    HowMany	how many multiple servers
    CIDList	list of connection ids
    RCList	NULL or pointer to array for individual return codes
    MCast	NULL if non-multicast; else points to multicast info
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
    PARM *_ptr;
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
    va_array = malloc(i * sizeof(PARM)); /* and then malloc the storage */
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
			    va_array[i].bd = va_arg(ap, long *); 
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
	    case C_END:
		    break;
	    case IN_MODE:
	    case IN_OUT_MODE:
		    switch(a_types->type) {
		    case RPC2_STRUCT_TAG:	_length += struct_len(&a_types, &args);
			    break;
			    
		    case RPC2_BULKDESCRIPTOR_TAG:
			    a_types->bound = 0;
			    SDescList = args[0].sedp;
			    break;
			    
		    default:		a_types->bound = 0;
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
    if (_rpc2val != RPC2_SUCCESS) return _rpc2val;
    
    /* Pack arguments */
    _ptr = (PARM *)_reqbuffer->Body;
    for(a_types = ArgTypes, args = va_array; a_types->mode != C_END; a_types++) {
	    if (a_types->mode != OUT_MODE) {
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

    _rpc2val = RPC2_MultiRPC(HowMany, CIDList, RCList, MCast, _reqbuffer, SDescList, MRPC_UnpackMulti, &arg_info, Timeout);
    
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
void pack_struct(ARG *a_types, PARM **args, PARM **ptr)
{
	ARG *field;
	PARM **strp, *str;
	int i, maxiterate;

	if (a_types->mode == IN_OUT_MODE) {
		str = *(*args)->structpp;
		strp = &str;
	}
	else if (a_types->mode == IN_MODE) {
		str = (*args)->structp;
		strp = &str;
	}
	else strp = args;
	
	if (a_types->bound != 0) {
		maxiterate = get_arraylen_pack(a_types-1, *args-1);
		for(i = 0; i < maxiterate; i++) {
			for(field = a_types->field; field->mode != C_END; field++) {
				if (field->type == RPC2_STRUCT_TAG)
					pack_struct(field, strp, ptr);
				else pack(field, strp, ptr);
			}
		}
	} else {
		for(field = a_types->field; field->mode != C_END; field++) {
			if (field->type == RPC2_STRUCT_TAG)
				pack_struct(field, strp, ptr);
			else pack(field, strp, ptr);
		}
	}
}


/* Packs the given type into the RequestBuffer */
void pack(ARG *a_types, PARM **args, PARM **_ptr)
  {
    RPC2_Byte byte;
    long _length;
    MODE mode = a_types->mode;
    PARM *arg = *args;
    RPC2_CountedBS *cbsbodyp;
    RPC2_BoundedBS *bbsbodyp;

    switch(a_types->type) {
	case RPC2_INTEGER_TAG:
			      if (mode == IN_OUT_MODE)
				(*_ptr)->integer = htonl(**arg->integerp);
			      else (*_ptr)->integer = htonl(arg->integer);
			      (*_ptr)++;
			      (*args)++;
			      break;
	   case RPC2_UNSIGNED_TAG:
			      if (mode == IN_OUT_MODE)
				(*_ptr)->unsgned = htonl(**arg->unsgnedp);
			      else (*_ptr)->unsgned = htonl(arg->unsgned);
			      (*_ptr)++;
			      (*args)++;
			      break;
	   case RPC2_BYTE_TAG:
			      if (a_types->bound != 0) {	/* Byte array */
				if (mode == IN_MODE)
				{
				  memcpy(*_ptr, arg->bytep, a_types->bound);
				  (*args)++;
				}
				else if(mode == IN_OUT_MODE)
				{
				  memcpy(*_ptr, *arg->bytep, a_types->bound);
				  (*args)++;
				}
				else if (mode == NO_MODE)
				{	/* structure field */
				  memcpy(*_ptr, &arg->byte, a_types->bound);
				  incr_struct_byte(a_types, args);
				}
#if SIZE == 4
			      (*_ptr) += (a_types->size) >> 2;
#else
			      (*_ptr) += a_types->size / SIZE;
#endif
			      }
			      else {				/* single byte */
				if (mode == IN_OUT_MODE) {
				   /* byte = **arg->integerp; */ /* bug fix */
				   byte = **(arg->bytep);
				   *(RPC2_Byte *)(*_ptr) = byte;
				   (*args)++;  /* bug fix */
				}
				else {
				   if (mode == NO_MODE) {	/* structure field */
				     *(RPC2_Byte *)(*_ptr) = *(RPC2_Byte *)arg;
				     incr_struct_byte(a_types, args);
				   }
				   else {			/* IN mode only */
				     byte = arg->integer;
				     *(RPC2_Byte *)(*_ptr) = byte;
				     (*args)++;
				   }
				}
				(PARM *)(*_ptr)++;
			      }
			      break;
	   case RPC2_ENUM_TAG:
			      if (mode == IN_OUT_MODE)
				(*_ptr)->integer = htonl((long)**arg->integerp);
			      else (*_ptr)->integer = htonl((long)arg->integer);
			      (*_ptr)++;
			      (*args)++;
			      break;
	   case RPC2_STRING_TAG:
			      if (mode == IN_OUT_MODE) {
				_length = strlen((*arg->stringp[0]));
				(*_ptr)->integer = htonl(_length);
				(*_ptr)++;
				(void) strcpy((RPC2_Byte *)(*_ptr), (*arg->stringp[0]));
			      }
			      else {
				_length = strlen(arg->string);
				(*_ptr)->integer = htonl(_length);
				(*_ptr)++;
				(void) strcpy((RPC2_Byte *)(*_ptr), arg->string);
			      }
			      *(RPC2_Byte *)((*_ptr)+_length) = '\0';
#if SIZE == 4
			      (*_ptr) += (_PAD(_length+1) >> 2);
			      /* (*_ptr) += ((a_types->size) >> 2) - 1; */
#else
			      (*_ptr) += (_PAD(_length+1) / SIZE);
			      /* (*_ptr) += (a_types->size / SIZE) - 1; */
#endif
			      (*args)++;
			      break;
	   case RPC2_COUNTEDBS_TAG:
			      if (mode == NO_MODE) {
				cbsbodyp = (RPC2_CountedBS *)arg;
			        _length = cbsbodyp->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
				memcpy(*_ptr, cbsbodyp->SeqBody, _length);
				/* Later *args is added by 4. Now add 4 for SeqLen */
				(*args)++; /* Ugly!! */
			      }
			      else if (mode == IN_OUT_MODE) {
			        _length = (*arg->cbsp)->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
			        memcpy(*_ptr, (*arg->cbsp)->SeqBody, _length);
			      }
			      else {
			        _length = arg->cbs->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
			        memcpy(*_ptr, arg->cbs->SeqBody, _length);
			      }
#if SIZE == 4
			      (*_ptr) += (_PAD(_length) >> 2);
			      /* (*_ptr) += ((a_types->size) >> 2) - 1; */
#else
			      (*_ptr) += (_PAD(_length) / SIZE);
			      /* (*_ptr) += (a_types->size / SIZE) - 1; */
#endif
			      (*args)++;
			      break;
	   case RPC2_BOUNDEDBS_TAG:
			      if (mode == NO_MODE) {
				bbsbodyp = (RPC2_BoundedBS *)arg;
			        ((*_ptr)++)->integer = htonl(bbsbodyp->MaxSeqLen);
			        _length = bbsbodyp->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
			        memcpy(*_ptr, bbsbodyp->SeqBody, _length);
				/* Later *args is added by 4. Now add 4 for SeqLen */
				(*args)++; /* Ugly!! */
				/* Later *args is added by 4. Now add 4 for MaxSeqLen */
				(*args)++; /* Ugly!! */
			      }
			      else if (mode == IN_OUT_MODE) {
			        ((*_ptr)++)->integer = htonl((*arg->bbsp)->MaxSeqLen);
			        _length = (*arg->bbsp)->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
			        memcpy(*_ptr, (*arg->bbsp)->SeqBody, _length);
			      }
			      else {
			        ((*_ptr)++)->integer = htonl(arg->bbs->MaxSeqLen);
			        _length = arg->bbs->SeqLen;
			        ((*_ptr)++)->integer = htonl(_length);
			        memcpy(*_ptr, arg->bbs->SeqBody, _length);
			      }
#if SIZE == 4
			      (*_ptr) += (_PAD(_length) >> 2);
			      /* (*_ptr) += ((a_types->size) >> 2) - 2; */
#else
			      (*_ptr) += (_PAD(_length) / SIZE);
			      /* (*_ptr) += (a_types->size / SIZE) - 2; */
#endif
			      (*args)++;
			      break;
	   case RPC2_ENCRYPTIONKEY_TAG:
			       if (mode == IN_OUT_MODE) {
			         memcpy(*_ptr, (*arg->keyp[0]), RPC2_KEYSIZE);
			       }
			       else {
			         memcpy(*_ptr, *arg->key, RPC2_KEYSIZE);
			       }
#if SIZE == 4
			      (*_ptr) += RPC2_KEYSIZE >> 2;
#else
			      (*_ptr) += RPC2_KEYSIZE / SIZE;
#endif
			      (*args)++;
			       break;
	   case RPC2_BULKDESCRIPTOR_TAG:
			      (*args)++;
			       break;
	   case RPC2_STRUCT_TAG:
				say(0, RPC2_DebugLevel, "MakeMulti (pack): RPC2_STRUCT_TAG encountered\n");
				break;
	   default:
			       say(0, RPC2_DebugLevel, "MakeMulti (pack): unknown type tag: %d\n", a_types->type);
	}
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
		str = *((*args)->structpp);
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
    PARM *_ptr;	/* holds rspbuffer */
    int ret;

    if (rpcval == RPC2_SUCCESS) {
       if(rspbuffer->Header.ReturnCode == RPC2_INVALIDOPCODE)
	  rpcval = RPC2_INVALIDOPCODE;
       else {
	  _ptr = (PARM *)rspbuffer->Body;
	  rpcval = rspbuffer->Header.ReturnCode;
	  for(a_types = ArgInfo->ArgTypes, args = ArgInfo->Args; a_types->mode != C_END; a_types++, args++) {
	    switch(a_types->mode){
		case IN_MODE:
			break;
		case OUT_MODE:
		case IN_OUT_MODE:
			if (a_types->type == RPC2_STRUCT_TAG) {
			   str = (PARM *) args->structpp[offset];
			   unpack_struct(a_types, &str, &_ptr, offset);
			}
			else unpack(a_types, args, &_ptr, offset);
			break;
		default:	assert(FALSE);
	    }
	  }
       }
    }

   /* Call client routine with arguments and RPC2 return code */
    args = ArgInfo->Args;
    if (ArgInfo->HandleResult)
	ret = mkcall(ArgInfo->HandleResult, ArgInfo->ArgCount, HowMany,
		     ConnHandleList, offset, rpcval, (int *)args);
    else ret = 0;
    if (rspbuffer != NULL) {
	RPC2_FreeBuffer(&rspbuffer);
    }
    return(ret);

 }


/* Returns the buffer length needed for the given argument, or -1 if unknown type.
 * Note that this routine modifies the static array of argument type descriptors 
 * defined in <subsys>.client.c by changing the value of the 'size' field. 
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
			else return(SIZE);	/* don't set (*a_types)->size for single char variable */
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
	case RPC2_ENUM_TAG:
			return ((*a_types)->size);
	case RPC2_STRING_TAG:
	     		if (mode == IN_OUT_MODE) 
			  return((*a_types)->size = SIZE+_PAD(strlen((*(*args)->stringp[0]))+1));
			else return((*a_types)->size = SIZE+_PAD(strlen((*args)->string)+1));
	case RPC2_COUNTEDBS_TAG:
			if (mode == NO_MODE) {
			  cbsbodyp = (RPC2_CountedBS *)(*args);
			  return((*a_types)->size = SIZE+_PAD(cbsbodyp->SeqLen));
			} else if (mode == IN_OUT_MODE) 
			  return((*a_types)->size = SIZE+_PAD((*(*args)->cbsp)->SeqLen));
			else return((*a_types)->size = SIZE+_PAD((*args)->cbs->SeqLen));
	case RPC2_BOUNDEDBS_TAG:
			if (mode == NO_MODE) {
			  bbsbodyp = (RPC2_BoundedBS *)(*args);
			  return((*a_types)->size = 2*SIZE+_PAD(bbsbodyp->SeqLen));
			} else if (mode == IN_OUT_MODE)
			  return((*a_types)->size = 2*SIZE+_PAD((*(*args)->bbsp)->SeqLen));
			else return((*a_types)->size = 2*SIZE+_PAD((*args)->bbs->SeqLen));
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
int get_arraylen_pack(ARG *a_types, PARM *args)
{
    int arraysize;
    switch(a_types->type) {
        case RPC2_INTEGER_TAG:
                        if (a_types->mode == IN_OUT_MODE)
			    arraysize = **args->integerp;
			else
			    arraysize = args->integer;
			return arraysize;
			/*NOTREACHED*/
			break;
        default:
			say(0, RPC2_DebugLevel, "MakeMulti: cannot pack array size\n");
			exit(-1);
    }
    /*NOTREACHED*/
}

int get_arraylen_unpack(ARG *a_types, PARM *ptr)
{
    switch(a_types->type) {
        case RPC2_INTEGER_TAG:
			return ntohl(ptr->integer);
			/*NOTREACHED*/
			break;
        default:
			say(0, RPC2_DebugLevel, "MakeMulti: cannot unpack array size\n");
			exit(-1);
    }
    /*NOTREACHED*/
}

void unpack(ARG *a_types, PARM *args, PARM **_ptr, long offset)
  {
     int _length;
     RPC2_CountedBS *cbsbodyp;
     RPC2_BoundedBS *bbsbodyp;
     MODE mode = a_types->mode;

	    switch(a_types->type) {
		case RPC2_INTEGER_TAG:
				if (mode != NO_MODE)
				   *(args->integerp[offset]) = ntohl((*_ptr)->integer);
				else args->integer = ntohl((*_ptr)->integer);
				(*_ptr)++;
				break;
		case RPC2_UNSIGNED_TAG:
				if (mode != NO_MODE)
				   *(args->unsgnedp[offset]) = ntohl((*_ptr)->unsgned);
				else args->unsgned = ntohl((*_ptr)->unsgned);
				(*_ptr)++;
				break;
		case RPC2_BYTE_TAG:
				if (a_types->bound != 0) {
				   if (mode == NO_MODE) {
				     memcpy(&(args->byte), *_ptr,
					    a_types->bound);
#if SIZE == 4
				     (*_ptr) += (a_types->size) >> 2;
#else
				     (*_ptr) += a_types->size / SIZE;
#endif
				   }
				   else {
				     memcpy(args->bytep[offset],
					    *_ptr, a_types->bound);
				     (*_ptr) ++;
				   }
				}
				else {
				   if (mode != NO_MODE)
				      *(args->bytep[offset]) = *(RPC2_Byte *)(*_ptr);
				   else args->byte = *(RPC2_Byte *)(*_ptr);
				   (*_ptr)++;
				}
				break;
		 case RPC2_ENUM_TAG:
				if (mode != NO_MODE) {
				   *(args->integerp[offset]) = ntohl((*_ptr)->integer);
				}
				else args->integer = ntohl((*_ptr)->integer);
				(*_ptr)++;
				break;
		 case RPC2_STRING_TAG:
				_length = ntohl((*_ptr)->integer) + 1;
				(*_ptr)++;
				if (mode != NO_MODE) {
				   memcpy(*(args->stringp[offset]), *_ptr,
					  _length);
				  (*args->stringp[offset])[_length - 1] = '\0';
				}
				else {
				   memcpy(args->string, *_ptr, _length);
				   args->string[_length - 1] = '\0';  /* used to be [length] */
				}
#if SIZE == 4
				(*_ptr) += (_PAD(_length)) >> 2;
#else
				(*_ptr) += (_PAD(_length)) / SIZE;
#endif
				break;
		case RPC2_COUNTEDBS_TAG:
				if (mode != NO_MODE) {
				  args->cbsp[offset]->SeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  memcpy(args->cbsp[offset]->SeqBody, *_ptr,
					 args->cbsp[offset]->SeqLen);
#if SIZE == 4
				  (*_ptr) += (_PAD(args->cbsp[offset]->SeqLen)) >> 2;
#else
				  (*_ptr) += (_PAD(args->cbsp[offset]->SeqLen)) / SIZE;
#endif
				}
				else {
				  cbsbodyp = (RPC2_CountedBS *)args;
				  cbsbodyp->SeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  memcpy(cbsbodyp->SeqBody, *_ptr,
					 cbsbodyp->SeqLen);
#if SIZE == 4
				  (*_ptr) += (_PAD(cbsbodyp->SeqLen)) >> 2;
#else
				  (*_ptr) += (_PAD(cbsbodyp->SeqLen)) / SIZE;
#endif
				}
				break;
		case RPC2_BOUNDEDBS_TAG:
				if (mode != NO_MODE) {
				  args->bbsp[offset]->MaxSeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  args->bbsp[offset]->SeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  memcpy(args->bbsp[offset]->SeqBody, *_ptr,
					 args->bbsp[offset]->SeqLen);
#if SIZE == 4
				  (*_ptr) += (_PAD(args->bbsp[offset]->SeqLen)) >> 2;
#else
				  (*_ptr) += (_PAD(args->bbsp[offset]->SeqLen)) / SIZE;
#endif
				}
				else {
				  bbsbodyp = (RPC2_BoundedBS *)args;
				  bbsbodyp->MaxSeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  bbsbodyp->SeqLen = ntohl((*_ptr)->integer);
				  (*_ptr)++;
				  memcpy(bbsbodyp->SeqBody, *_ptr,
					 bbsbodyp->SeqLen);
#if SIZE == 4
				  (*_ptr) += (_PAD(bbsbodyp->SeqLen)) >> 2;
#else
				  (*_ptr) += (_PAD(bbsbodyp->SeqLen)) / SIZE;
#endif
				}
		case RPC2_BULKDESCRIPTOR_TAG:
				break;
		case RPC2_STRUCT_TAG:
				say(0, RPC2_DebugLevel, "Unpack: encountered struct\n");
				break;
		case RPC2_ENCRYPTIONKEY_TAG:
				if (mode == IN_OUT_MODE) {
				memcpy(args->keyp[offset], *_ptr, RPC2_KEYSIZE);
				}
				else memcpy(*(args->key), *_ptr, RPC2_KEYSIZE);
#if SIZE == 4
				(*_ptr) += (_PAD(RPC2_KEYSIZE)) >> 2;
#else
				(*_ptr) += (_PAD(RPC2_KEYSIZE)) / SIZE;
#endif
				break;
		default:
				say(0, RPC2_DebugLevel, "UnpackMulti (unpack): unknown tag: %d\n", a_types->type);
	    }
  }


void unpack_struct(ARG *a_types, PARM **args, PARM **_ptr, long offset)
{
    ARG *field;
    PARM **strp, *str;
    int i, maxiterate;

    if (a_types->mode != NO_MODE) {
	str = *args;
	strp = &str;
    }
    else strp = args;

    if (a_types->bound != 0) {
        /* Array size should be stored before array structures */
        maxiterate = get_arraylen_unpack(a_types-1, *_ptr-1);
        for(i = 0; i < maxiterate; i++) {
	    for(field = a_types->field; field->mode != C_END; field++) {
	        if (field->type == RPC2_STRUCT_TAG)
	            unpack_struct(field, strp, _ptr, -1);
	        else {
	            unpack(field, *strp, _ptr, offset);
		    switch (field->type) {
		      case RPC2_BYTE_TAG:
		        incr_struct_byte(field, strp);
			break;
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
	}
    } else {
	for(field = a_types->field; field->mode != C_END; field++) {
	    if (field->type == RPC2_STRUCT_TAG)
	        unpack_struct(field, strp, _ptr, -1);
	    else {
	        unpack(field, *strp, _ptr, offset);
		switch (field->type) {
		  case RPC2_BYTE_TAG:
		    incr_struct_byte(field, strp);
		    break;
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
    }
}

/* This should only be called for structure fields, never for top level arguments */
void incr_struct_byte(ARG *a_types, PARM **args)
  {
	*(char **)args += (a_types->bound) ? (a_types->bound) : 1;
	if (a_types[1].type == RPC2_BYTE_TAG) return;
	byte_pad(args);
  }


void byte_pad(PARM **args)
{
#ifdef sun
				     *(char **) args = (char *)_PADWORD((int) *args);
#endif
#ifdef ibm032
				     *(char **) args = (char *)_PADLONG((int) *args);
#endif
#ifdef vax
				     *(char **) args = (char *)_PADLONG((int) *args);
#endif
#ifdef mips
				     *(char **) args = (char *)_PADLONG((int) *args);
#endif
}

