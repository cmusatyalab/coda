#ifndef _BLURB_
#define _BLURB_
#ifdef undef
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/sys/pioctl.s,v 1.1.1.1 1996/12/09 19:09:18 rvb Exp";
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

SYSCALL(pioctl)
	RET
#endif

#ifdef	mips
#include <mips/asm.h>
#include <syscall.h>

SYSCALL(pioctl)
	RET
	END(pioctl)
#endif	mips


#ifdef	i386
#include <sys/syscall.h>
#include <machine/asm.h>
#ifndef __NetBSD__
#ifdef __STDC__
#define SYSCALL(x)	ENTRY(x); movl	$SYS_ ## x, %eax; SVC; jb LCL(cerror)
#else
#define SYSCALL(x)	ENTRY(x); movl	$(SYS_/**/x), %eax; SVC; jb LCL(cerror)
#endif


	.globl	LCL(cerror)
#else	__NetBSD__
#include "SYS.h"	
#endif	__NetBSD__

SYSCALL(pioctl)
	ret

#endif	i386

#ifdef vax
	.globl	_errno
kerror:	movl	r0,_errno
	mnegl	$1,r0
	ret
	.globl	_pioctl
_pioctl:
	.word	0x0
	chmk	$157
	blssu	kerror
	ret
#endif
#ifdef sun3
| pioctl system call
| rns, 3/85.

	.globl	cerror
error:	jmp	cerror

	.globl	_pioctl
_pioctl:
	pea	168:w
	trap	#0
	bcss	error
	rts
#endif
#ifdef romp
	.data
	.globl	.oVncs
	.set		.oVncs,0
	.globl	_pioctl
_pioctl:
	.long		_.pioctl
	.text
error:
	.globl	_errno
	get	r5,$_errno
	sts	r2,0(r5)
	cal	r2,-1(r0)
	br	r15

	.globl	_.pioctl
_.pioctl:
	cal	r5,159(r0)
	svc	0(r5)
	jtb	error
	br	r15
#endif

