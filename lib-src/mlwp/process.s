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

static char *rcsid = "$Header: /home/braam/src/lib-src/mlwp/process.s,v 1.1 1996/12/05 18:58:46 braam Exp braam $";
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

#ifdef OLDLWP

#if defined(sun3) || defined(mc68000) || defined(mc68020) || defined(mc68030)
	.data

/*
#
#	Process assembly language assist for Sun's.
#
*/

	.text
	.even

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/

	.globl	_PRE_Block

topstack =	0

/* Stuff to allow saving/restoring registers */
nregs	=	13
regs	=	0x3ffe			| d1-d7 & a0-a5

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/

/* Stack offsets of arguments */
f	=	8
area1	=	12
newsp	=	16

	.globl	_savecontext
_savecontext:
	movb	#1,_PRE_Block		| Do not allow any interrupt finagling
	link	a6,#-(nregs*4)		| Save frame pointer & ...
					| ... allocate space for nregs registers
/* Save registers */
	moveml	#regs,sp@

	movl	a6@(area1),a0		| a0 = base of savearea
	movl	sp,a0@(topstack)	| area->topstack = sp
	movl	a6@(newsp),d0		| Get new sp
	jeq	noswitch		| If newsp == 0, no stack switch
	movl	d0,sp			| Switch to new stack
noswitch:
	movl	a6@(f),a0		| a0 = f
	jbsr	a0@			| f()

/* It is impossible to be here, so abort() */

	jbsr	_abort

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
area2	=	8

	.globl _returnto
_returnto:
	link	a6,#0
	movl	a6@(area2),a0		| Base of savearea
	movl	a0@(topstack),sp	| Restore sp
/* Restore registers */
	moveml	sp@,#regs

	addl	#(nregs*4),sp
	movl	sp,a6			| Argghh...be careful here
	unlk	a6
	clrb	_PRE_Block
	rts				| Return to previous process
#endif sun3

#if defined(sun4) || defined(sparc)
#include <sun4/asm_linkage.h>
#include <sun4/trap.h>

SAVED_PC	= (0*4)
SAVED_FP	= (1*4)
SAVESIZE	= (2*4)

	.seg	"data"
	.globl	NAME(PRE_Block)
	.seg	"text"
	.align	4

ENTRY(savecontext)
	save	%sp, -SA(MINFRAME + SAVESIZE), %sp
	mov	0x1, %o0
	sethi	%hi(NAME(PRE_Block)), %o1
	stb	%o0, [%o1 + %lo(NAME(PRE_Block))]
	st	%i7, [%sp + MINFRAME + SAVED_PC]
	st	%fp, [%sp + MINFRAME + SAVED_FP]
	tst	%i2
	be	1f
	st	%sp, [%i1]
	and	%i2, -STACK_ALIGN, %sp
	sub	%sp, SA(MINFRAME), %sp
1:
	call	.ptr_call, 0
	mov	%i0, %g1
	unimp	0

ENTRY(returnto)
	t	ST_FLUSH_WINDOWS
	ld	[%o0], %sp
	ld	[%sp + MINFRAME + SAVED_FP], %fp
	ld	[%sp + MINFRAME + SAVED_PC], %o7
	sethi	%hi(NAME(PRE_Block)), %o0
	stb	%g0, [%o0 + %lo(NAME(PRE_Block))]
	retl
	restore
#endif

#ifdef ibm032
	.data
	.globl	.oVncs
	.set		.oVncs,0

	.globl	_savecontext
_savecontext:
	.long		_.savecontext

	.globl	_returnto
_returnto:
	.long		_.returnto

|
|	Process assembly language assist for Sailboats.
|

	.text
	.align 2

|
| struct savearea {
|	char	*topstack;
| }
|

| Offsets of fields
.set topstack,0

| Stuff to allow saving/restoring registers
.set regspace,64
.set freg,0

|
| savecontext(f, area1, newsp)
|    int (*f)(); struct savearea *area1; char *newsp;
|

	.globl	_.savecontext
_.savecontext:
	ai	sp,sp,-regspace		| Save frame pointer & ...
					| ... allocate space for 16 registers
| Save registers
	stm	r0,0(sp)			| Change this if save fewer regs.
| Set preemption semaphore
	get	r6,$1
	get	r7,$_PRE_Block
	putc	r6,0(r7)			| PRE_Block = 1
| r3 = base of savearea
	put	sp,topstack(r3)		| area1->topstack = sp
| New sp is in r4.
	cis	r4,0
	be	L1			| If newsp == 0, no stack switch
	cas	sp,r4,r0			| Switch to new stack
L1:
	get	r6,0(r2)			| r2 = _f
	balrx	r15,r6			| f()
	cas	r0,r2,r0

|
| returnto(area2)
|     struct savearea *area2;
|

	.globl _.returnto
_.returnto:
	get	sp,topstack(r2)
| Now in the context of the savecontext stack to be restored.
| Start with the registers...
| Clear preemption semaphore
	get	r6,$0
	get	r7,$_PRE_Block
	putc	r6,0(r7)			| PRE_Block = 0
	lm	r0,0(sp)		| Change if saving fewer regs.
	brx	r15		| Return to previous process
	ai	sp,sp,regspace
 .data
 .ltorg
#endif

#ifdef vax
	.data

	.text

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/

	.set	topstack,0

/* Stuff to allow saving/restoring registers */

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/

/* Stack offsets of arguments */
	.set	f,4
	.set	area1,8
	.set	newsp,12

.globl	_PRE_Block
.globl	_savecontext

_savecontext:
	.word 0x0ffc	# Save regs R2-R11
	movb	$1,_PRE_Block		# Critical section for preemption code
   	pushl	ap			# save old ap
	pushl	fp			# save old fp    
	movl	area1(ap),r0		# r0 = base of savearea
	movl	sp,topstack(r0)		# area->topstack = sp
	movl	newsp(ap),r0		# Get new sp
	beql	L1			# if new sp is 0, do not change stacks
	movl	r0,sp			# else switch to new stack
L1:
	movl	f(ap),r1		# r1 = f
	calls	$0,0(r1)		# f()

/* It is impossible to be here, so abort() */

	calls	$0,_abort

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
	.set	area2,4

	.globl _returnto
_returnto:
	.word	0x0			# Who cares about these regs?
	movl	area2(ap),r0		# r0 = address of area2
	movl	topstack(r0),sp		# Restore sp
	movl	(sp)+,fp		# Restore fp
	movl	(sp)+,ap		# ,,,,
	clrb	_PRE_Block		# End of preemption critical section
	ret

	pushl	$1234			# I will gloat, Kazar
	calls	$0,_abort
#endif

#ifdef mips
/* Code for MIPS R2000/R3000 architecture
 * Written by Zalman Stern April 30th, 1989.
 */
#if	MACH
#include <mips/regdef.h>
#define s8	fp
#else
#include <regdef.h>
#ifndef s8
#define s8	fp
#endif  s8
#endif
#define regspace 9 * 4 + 4 + 6 * 8
#define floats 0
#define registers floats + 6 * 8
#define returnaddr regspace - 4
#define topstack 0
	.globl savecontext /* MIPS' C compiler doesn't prepend underscores. */
	.ent savecontext /* Insert debugger information. */
savecontext:
	li	t0, 1
	.extern	PRE_Block
	sb	t0, PRE_Block
	subu	sp, regspace
	.frame	sp, regspace, ra
/* Save registers. */
	sw	s0, registers + 0(sp)
	sw	s1, registers + 4(sp)
	sw	s2, registers + 8(sp)
	sw	s3, registers + 12(sp)
	sw	s4, registers + 16(sp)
	sw	s5, registers + 20(sp)
	sw	s6, registers + 24(sp)
	sw	s7, registers + 28(sp)
	sw	s8, registers + 32(sp)
/* Save return address */
	sw	ra, returnaddr(sp)
	.mask	0xc0ff0000, -4
/* Need to save floating point registers? */
	s.d	$f20, floats + 0(sp)
	s.d	$f22, floats + 8(sp)
	s.d	$f24, floats + 16(sp)
	s.d	$f26, floats + 24(sp)
	s.d	$f28, floats + 32(sp)
	s.d	$f30, floats + 40(sp)
	.fmask	0x55400000, regspace
	sw	sp, topstack(a1)
	beq	a2, $0, samestack
	li	t0, -8
	and	sp, t0, a2
samestack:
	jal	a0
	.end	savecontext

	.globl	returnto
	.ent	returnto
returnto:
	lw	sp, topstack(a0)
	lw	s0, registers + 0(sp)
	lw	s1, registers + 4(sp)
	lw	s2, registers + 8(sp)
	lw	s3, registers + 12(sp)
	lw	s4, registers + 16(sp)
	lw	s5, registers + 20(sp)
	lw	s6, registers + 24(sp)
	lw	s7, registers + 28(sp)
	lw	s8, registers + 32(sp)
/* Save return address */
	lw	ra, returnaddr(sp)
/* Need to save floating point registers? */
	l.d	$f20, floats + 0(sp)
	l.d	$f22, floats + 8(sp)
	l.d	$f24, floats + 16(sp)
	l.d	$f26, floats + 24(sp)
	l.d	$f28, floats + 32(sp)
	l.d	$f30, floats + 40(sp)
        addu    sp, regspace
	sb	$0, PRE_Block
	j	ra
	.end	returnto
#endif mips

#ifdef	i386
		
/*
   savecontext(f, area1, newsp)
	int (*f)();
 	struct savearea *area1;
	char *newsp;
*/
	
#ifdef LINUX
#include <linux/linkage.h>	
#define SYMB(name)  ENTRY(name)
#define EXT(x) SYMBOL_NAME(x)
#else
#ifdef __STDC__	
#define SYMB(x)  _##x:
#define EXT(x)	_##x
#else
#define SYMB(x)  _/**/x:
#define EXT(x)	_/**/x
#endif
#endif	



#define	f	36
#define area1	40
#define newsp	44
#define topstack 0

        .globl  _PRE_Block
	.text

	.align	2
	.globl	_savecontext
SYMB(savecontext)
	movl	$1, EXT(PRE_Block)	/* Set PRE_Block to 1 to prevent interruption. */

	pusha				/* Save registers on the stack. */
		
	movl	area1(%esp), %eax	/* Load address of savearea. */
	movl	%esp, topstack(%eax)	/* Store sp in savearea. */

	movl	f(%esp), %ebp		/* Load f */
	movl	newsp(%esp), %ecx	/* Load the new sp value. */
	cmpl	$0, %ecx
	je	L1			/* Don't change the stack if newsp is zero. */
	movl	%ecx, %esp

L1:	call	*%ebp			/* f(); */

	call	EXT(abort)			/*  Shouldn't get here anyway. */


/*
  returnto(area2)
     struct savearea *area2;
*/
#define	area2	4


	.align	2
	.globl	_returnto
SYMB(returnto)
	movl	area2(%esp), %edx	/* address of save area. */
	movl	topstack(%edx), %esp	/* Restore stack pointer. */
	popa				/* Restore registers. */
	movl	$0, EXT(PRE_Block)		/* Clear critical condition */
	ret

#endif 	i386


#ifdef luna88k
/* Luna88K Code from Dan Stodolsky via Brad White */

	global	_PRE_Block

	text
	align	4
	global 	_savecontext

_savecontext:
/* Set semaphore */
	or	r12, r0, 1
	or.u	r13, r0, hi16(_PRE_Block)
	st.b	r12, r13, lo16(_PRE_Block)
/* Allocate stack */
	subu	r31, r31, 56
/* Save return address */
	st	r1, r31, 4
/* Save frame pointer */
	st	r30, r31, 0
/* Set the frame pointer (do I need this?) */
/*	or	r30, r0, r31*/
/* Save registers */
	st	r14, r31, 8
	st	r15, r31, 12
	st	r16, r31, 16
	st	r17, r31, 20
	st	r18, r31, 24
	st	r19, r31, 28
	st	r20, r31, 32
	st	r21, r31, 36
	st	r22, r31, 40
	st	r23, r31, 44
	st	r24, r31, 48
	st	r25, r31, 52
/* Magic */
	st	r31, r3, 0	/* area->topstack = sp */
	bcnd	eq0, r4, @L2	
	or	r31, r0, r4
	and	r31, r31, 0xfff8
@L2:
	jsr	r2

	text
	align 4
	global _returnto

_returnto:
/* Restore stack pointer */
	ld	r31, r2, 0
/* Restore return address */
	ld	r1, r31, 4
/* Restore frame pointer */
	ld	r30, r31, 0
/* Restore registers */
	ld	r14, r31, 8
	ld	r15, r31, 12
	ld	r16, r31, 16
	ld	r17, r31, 20
	ld	r18, r31, 24
	ld	r19, r31, 28
	ld	r20, r31, 32
	ld	r21, r31, 36
	ld	r22, r31, 40
	ld	r23, r31, 44
	ld	r24, r31, 48
	ld	r25, r31, 52
/* Clear the semaphore */
	or.u	r13, r0, hi16(_PRE_Block)
	st.b	r0, r13, lo16(_PRE_Block)
/* Restore stack size */
	addu	r31, r31, 56
	jmp	r1

#endif	/* luna88k */

#endif OLDLWP
