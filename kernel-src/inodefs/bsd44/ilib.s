#ifndef _BLURB_
#define _BLURB_
#ifdef undef
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

          Copyright (c) 1987-1995 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: ilib.s,v 1.1 96/11/22 13:39:42 raiff Exp $";
#endif undef
#endif /*_BLURB_*/

/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#if defined(sun4) || defined(sparc)
#include <syscall.h>
#define CERROR(free_reg,x) \
	bcc 	_noerr/**/x; \
	.empty; \
	sethi	%hi(cerror), %free_reg;\
	or	%free_reg, %lo(cerror), %free_reg;\
	jmp	%free_reg;\
	.empty;\
_noerr/**/x: nop;
	
#define ENTRY(x) \
	.global	_/**/x; \
_/**/x:

#define SYSCALL(x)  ENTRY(x); mov SYS_/**/x, %g1; t 0; CERROR(o5,x)

#define RET	retl; nop;

	.global	cerror

SYSCALL(icreate)
	RET

SYSCALL(iopen)
	RET

SYSCALL(iread)
	RET

SYSCALL(iwrite)
	RET

SYSCALL(iinc)
	RET

SYSCALL(idec)
	RET
#endif

#ifdef	mips
#include <mips/asm.h>
#include <syscall.h>

SYSCALL(icreate)
	RET
	END(icreate)

SYSCALL(iopen)
	RET
	END(iopen)

SYSCALL(iread)
	RET
	END(iread)

SYSCALL(iwrite)
	RET
	END(iwrite)

SYSCALL(iinc)
	RET
	END(iinc)

SYSCALL(idec)
	RET
	END(idec)
#endif

#ifdef	i386
#include <sys/syscall.h>
#include <machine/asm.h>
#ifndef __NetBSD__
#define SYSCALL(x)	ENTRY(x); movl	$SYS_/**/x, %eax; SVC; jb LCL(cerror)

	.globl	LCL(cerror)

#else	__NetBSD__
#include "SYS.h"	
#endif	__NetBSD__

SYSCALL(icreate)
	ret

SYSCALL(iopen)
	ret

SYSCALL(iread)
	ret

SYSCALL(iwrite)
	ret

SYSCALL(iinc)
	ret

SYSCALL(idec)
	ret
#endif
#ifdef vax
	.globl	_errno
kerror:		movl	r0,_errno
	mnegl	$1,r0
	ret
	.globl	_icreate
_icreate:
	.word	0x0
	chmk	$151
	blssu	kerror
	ret
	.globl	_iopen
_iopen:
	.word	0x0
	chmk	$152
	blssu	kerror
	ret
	.globl	_iread
_iread:
	.word	0x0
	chmk	$153
	blssu	kerror
	ret
	.globl	_iwrite
_iwrite:
	.word	0x0
	chmk	$154
	blssu	kerror
	ret
	.globl	_iinc
_iinc:
	.word	0x0
	chmk	$155
	blssu	kerror
	ret
	.globl	_idec
_idec:
	.word	0x0
	chmk	$156
	blssu	kerror
	ret
#endif
#ifdef sun3
| Inode system calls
| RNS, 3/85.

	.globl	cerror
error:	jmp	cerror

	.globl	_icreate
_icreate:
	pea	170:w
	trap	#0
	bcss	error
	rts
	
	.globl	_iopen
_iopen:
	pea	171:w
	trap	#0
	bcss	error
	rts
	
	.globl	_iread
_iread:
	pea	172:w
	trap	#0
	bcss	error
	rts
	
	.globl	_iwrite
_iwrite:
	pea	173:w
	trap	#0
	bcss	error
	rts
	
	.globl	_iinc
_iinc:
	pea	174:w
	trap	#0
	bcss	error
	rts
	
	.globl	_idec
_idec:
	pea	175:w
	trap	#0
	bcss	error
	rts
#endif
#ifdef romp
	.globl	.oVncs
	.set		.oVncs,0
	.data
	.globl	_icreate
_icreate:
	.long	_.icreate
	.globl	_iopen
_iopen:
	.long	_.iopen
	.globl	_iread
_iread:
	.long	_.iread
	.globl	_iwrite
_iwrite:
	.long _.iwrite
	.globl	_iinc
_iinc:
	.long _.iinc
	.globl	_idec
_idec:
	.long _.idec

	.text
error:
	.globl	_errno
	get	r5,$_errno
	sts	r2,0(r5)
	cal	r2,-1(r0)
	br	r15

	.globl	_.icreate
_.icreate:
	svc	161(r0)
	jtb	error
	br	r15

	.globl	_.iopen
_.iopen:
	svc	154(r0)
	jtb	error
	br	r15

	.globl	_.iread
_.iread:
	svc	155(r0)
	jtb	error
	br	r15

	.globl	_.iwrite
_.iwrite:
	svc	156(r0)
	jtb	error
	br	r15

	.globl	_.iinc
_.iinc:
	svc	157(r0)
	jtb	error
	br	r15

	.globl	_.idec
_.idec:
	svc	158(r0)
	jtb	error
	br	r15
#endif
