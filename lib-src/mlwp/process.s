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
#ifdef __linux__
/* This info from Miguel de Icaza (and SunOS header files/libc) */
#define STACK_ALIGN 8
#define WINDOWSIZE (4*16)
#define ARGPUSHSIZE (6*4)
#define MINFRAME  (WINDOWSIZE+ARGPUSHSIZE+4) /* min frame */
#define SA(X)     (((X)+(STACK_ALIGN-1)) & ~(STACK_ALIGN-1))
#define NAME(x) x
#define ENTRY(x) .type x,@function; .global x; x:
#include <asm/traps.h>
#else
#include <sun4/asm_linkage.h>
#include <sun4/trap.h>
#endif

SAVED_PC	= (0*4)
SAVED_FP	= (1*4)
SAVESIZE	= (2*4)

	.seg	"data"
	.globl	NAME(PRE_Block)
	.seg	"text"
	.align	4

ENTRY(savecontext)
	save	%sp, -SA(MINFRAME + SAVESIZE), %sp
	t	ST_FLUSH_WINDOWS
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

#ifdef __linux__
ENTRY(.ptr_call)
	jmp %g1
	nop
#endif
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
	
#ifdef	__linux__
#include <linux/linkage.h>	
#define SYMB(name)  ENTRY(name)
#define EXT(x) SYMBOL_NAME(x)

#elif	defined(__BSD44__)

#ifdef __STDC__

#if	defined(__FreeBSD__) && defined(__ELF__)
#include <machine/asm.h>
#define SYMB(x)	ENTRY(x)
#define EXT(x)	CNAME(x)

#elif	defined(__FreeBSD__)
#define SYMB(x) .align 4;  .globl _##x; _##x:
#define EXT(x) _##x

#else	/* defined(__NetBSD__) */
#include <machine/asm.h>
#define SYMB(x)  ENTRY(x)
#define EXT(x)	_C_LABEL(x)
#endif	/*__FreeBSD_version*/

#else	/*!__STDC__*/
#define SYMB(x)  _/**/x:
#define EXT(x)	_/**/x
#endif	/*__STDC__*/

#else

/* some kind of win32 machine */
#define SYMB(x) .align 4;  .globl _##x; _##x:
#define EXT(x) _##x
	
#endif	/* ! __linux__ */



#define	f	36
#define area1	40
#define newsp	44
#define topstack 0

        .globl  EXT(PRE_Block)
	.text
SYMB(savecontext)
	movl	$1, EXT(PRE_Block)	/* Set PRE_Block to 1 to prevent interruption. */

	pusha				/* Save registers on the stack. */
		
	movl	area1(%esp), %eax	/* Load address of savearea. */
	movl	%esp, topstack(%eax)	/* Store sp in savearea. */

	movl	f(%esp), %eax		/* Load f */
	movl	newsp(%esp), %ecx	/* Load the new sp value. */
	cmpl	$0, %ecx
	je	L1			/* Don't change the stack if newsp is zero. */
	movl	%ecx, %esp

L1:	xorl	%ebp, %ebp		/* clear stackframe */
	call	*%eax			/* f(); */

	call	EXT(abort)			/*  Shouldn't get here anyway. */


/*
  returnto(area2)
     struct savearea *area2;
*/
#define	area2	4

SYMB(returnto)
	movl	area2(%esp), %edx	/* address of save area. */
	movl	topstack(%edx), %esp	/* Restore stack pointer. */
	popa				/* Restore registers. */
	movl	$0, EXT(PRE_Block)		/* Clear critical condition */
	ret

#endif 	/* i386 */


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

#ifdef __ns32k__

#ifdef __STDC__	
#define SYMB(x)  _##x:
#define EXT(x)	_##x
#else
#define SYMB(x)  _/**/x:
#define EXT(x)	_/**/x
#endif

/*
   savecontext(f, area1, newsp)
	int (*f)();
 	struct savearea *area1;
	char *newsp;
*/

/* Stack offsets of arguments */
f	=	8
area1	=	12
newsp	=	16

	.globl  _PRE_Block
	.globl	_abort
	.text

	.align	2
	.globl	EXT(savecontext)
SYMB(savecontext)
	movb	1, _PRE_Block(pc)	# Do not allow any interrupt finagling 
	enter	[r0,r1,r2,r3,r4,r5,r6,r7], 0	# save registers
	sprd	fp, r0			# push frame pointer
	movd	r0, tos
	movd	area1(fp), r0		# save sp to area1
	sprd	sp, 0(r0)
	movd	f(fp), r0		# get f()
	movd	newsp(fp), r1		# Get new sp
	cmpd	0, r1
	beq	noswitch		# Set if sp != 0
	lprd	sp, r1
noswitch:
	jsr	0(r0)

/*	should never get here ... */
	jsr	_abort
		
	
/*
  returnto(area2)
     struct savearea *area2;
*/
area2	=	8
	
	.globl EXT(returnto)
SYMB(returnto)
	enter	[], 0	
	movd	area2(fp), r0
	lprd	sp, 0(r0)
	movd	tos, r0
	lprd	fp, r0
	exit	[r0,r1,r2,r3,r4,r5,r6,r7]
	movb	0, _PRE_Block(pc)
	ret	0
		
#endif /* __ns32k__ */

#ifdef __arm32__
#ifdef __STDC__	
#define SYMB(x)  _##x:
#define EXT(x)	_##x
#else
#define SYMB(x)  _/**/x:
#define EXT(x)	_/**/x
#endif

/* register definitions */
fp	.req	r11
ip	.req	r12
sp	.req	r13
lp	.req	r14
pc	.req	r15

/*
   savecontext(f, area1, newsp)
	int (*f)();
 	struct savearea *area1;
	char *newsp;
*/

/* Arguments appear as:  f in r0, area1 in r1, newsp in r2 */

	.globl  _PRE_Block
	.globl	_abort
	.text
	.align	0
	.globl	EXT(savecontext)
	.type   EXT(savecontext), #function
SYMB(savecontext)
	@ build the frame
	mov 	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
	@ stack r0 - r10, current fp
	stmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, fp}
	str	sp, [r1, #0]
	@ set _PRE_Block
	ldr	r3, L1
	mov	r4, #1
	str	r4, [r3, #0]
	@ check if newsp is zero
	movs	r2, r2
	movne	sp, r2
	@ call function ...
	mov	pc, r0

/*	should never get here ... */
	bl	_abort

L1:	.word _PRE_Block		
	
/*
  returnto(area2)
     struct savearea *area2;
*/

/* area2 is in r0. */
	
	.globl EXT(returnto)
	.type  EXT(returnto), #function
SYMB(returnto)
	@ clear _PRE_Block
	ldr	r3, L1
	mov	r4, #0
	str	r4, [r3, #0]
	@ restore r0-r10, fp
	ldr	r0, [r0, #0]
	ldmfd	r0, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, fp}
	@ return from function call
	ldmea	fp, {fp, sp, pc}
	
#endif /* __arm32__ */

#endif OLDLWP

#if defined(__powerpc__)

/* Comments:
 *    1. Registers R10..R31 and CR0..CR7 are saved
 *    2. "struct savearea" must hold at least 3 pointers (long)
 *    3. This code will only work on 32 bit machines (601..604), not 620
 *    4. No floating point registers are saved
 *    5. The save stack "frame" is bigger than absolutely necessary.  The
 *       PowerPC [AIX] ABI needs this extra space.
 */


/* Mach-O assemblers */
#if !defined(NeXT) && !defined(__APPLE__)
#define r0    0
#define r1    1
#define r2    2
#define r3    3
#define r4    4
#define r5    5
#define r6    6
#define r7    7
#define r8    8
#define r9    9
#define r10   10
#define r11   11
#define r12   12
#define r13   13
#define r14   14
#define r15   15
#define r16   16
#define r17   17
#define r18   18
#define r19   19
#define r20   20
#define r21   21
#define r22   22
#define r23   23
#define r24   24
#define r25   25
#define r26   26
#define r27   27
#define r28   28
#define r29   29
#define r30   30
#define r31   31
#endif /* !NeXT && !__APPLE__ */


/*
 * savecontext(int (*f)(), struct savearea *save, char *newsp)
 */

#define FRAME_SIZE    (32*4)+(8*4)
#define FRAME_OFFSET  (8*4)

#define TOP_OF_STACK  (0*4)
#define RETURN                (1*4)
#define CCR           (2*4)

#if defined(NeXT) || defined(__APPLE__)
      .globl  _savecontext
_savecontext:
      lis     r9,ha16(_PRE_Block)     /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      .globl  savecontext
savecontext:
      lis     r9,PRE_Block@ha         /* Disable interrupt fiddling */
      li      r8,1
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      subi    r1,r1,FRAME_SIZE
      mfcr    r9
      stw     r9,CCR(r4)
      stw     r10,10*4+FRAME_OFFSET(r1)       /* Save registers */
      stw     r11,11*4+FRAME_OFFSET(r1)
      stw     r12,12*4+FRAME_OFFSET(r1)
      stw     r13,13*4+FRAME_OFFSET(r1)
      stw     r14,14*4+FRAME_OFFSET(r1)
      stw     r15,15*4+FRAME_OFFSET(r1)
      stw     r16,16*4+FRAME_OFFSET(r1)
      stw     r17,17*4+FRAME_OFFSET(r1)
      stw     r18,18*4+FRAME_OFFSET(r1)
      stw     r19,19*4+FRAME_OFFSET(r1)
      stw     r20,20*4+FRAME_OFFSET(r1)
      stw     r21,21*4+FRAME_OFFSET(r1)
      stw     r22,22*4+FRAME_OFFSET(r1)
      stw     r23,23*4+FRAME_OFFSET(r1)
      stw     r24,24*4+FRAME_OFFSET(r1)
      stw     r25,25*4+FRAME_OFFSET(r1)
      stw     r26,26*4+FRAME_OFFSET(r1)
      stw     r27,27*4+FRAME_OFFSET(r1)
      stw     r28,28*4+FRAME_OFFSET(r1)
      stw     r29,29*4+FRAME_OFFSET(r1)
      stw     r30,30*4+FRAME_OFFSET(r1)
      stw     r31,31*4+FRAME_OFFSET(r1)
      stw     r1,TOP_OF_STACK(r4)
      cmpi    0,r5,0                          /* New stack specified? */
      mflr    r0
      stw     r0,RETURN(r4)
      mtlr    r3
      beq     L1                            /* No - don't muck with pointer */

    /*  mr      r1,r5 */
      addi    r1,r5,-FRAME_OFFSET            /* leave space for silly linkage info */
L1:   blr                                     /* Return */

/*
 * returnto(struct savearea *area)
 */
#if defined(NeXT) || defined(__APPLE__)
      .globl  _returnto
_returnto:
#else	
      .globl  returnto
returnto:
#endif /* NeXT || __APPLE__ */
      lwz     r1,TOP_OF_STACK(r3)             /* Update stack pointer */
      lwz     r0,RETURN(r3)                   /* Get return address */
      mtlr    r0
      lwz     r4,CCR(r3)
      mtcrf   0xFF,r4
      lwz     r10,10*4+FRAME_OFFSET(r1)       /* Restore registers */
      lwz     r11,11*4+FRAME_OFFSET(r1)
      lwz     r12,12*4+FRAME_OFFSET(r1)
      lwz     r13,13*4+FRAME_OFFSET(r1)
      lwz     r14,14*4+FRAME_OFFSET(r1)
      lwz     r15,15*4+FRAME_OFFSET(r1)
      lwz     r16,16*4+FRAME_OFFSET(r1)
      lwz     r17,17*4+FRAME_OFFSET(r1)
      lwz     r18,18*4+FRAME_OFFSET(r1)
      lwz     r19,19*4+FRAME_OFFSET(r1)
      lwz     r20,20*4+FRAME_OFFSET(r1)
      lwz     r21,21*4+FRAME_OFFSET(r1)
      lwz     r22,22*4+FRAME_OFFSET(r1)
      lwz     r23,23*4+FRAME_OFFSET(r1)
      lwz     r24,24*4+FRAME_OFFSET(r1)
      lwz     r25,25*4+FRAME_OFFSET(r1)
      lwz     r26,26*4+FRAME_OFFSET(r1)
      lwz     r27,27*4+FRAME_OFFSET(r1)
      lwz     r28,28*4+FRAME_OFFSET(r1)
      lwz     r29,29*4+FRAME_OFFSET(r1)
      lwz     r30,30*4+FRAME_OFFSET(r1)
      lwz     r31,31*4+FRAME_OFFSET(r1)
#if defined(NeXT) || defined(__APPLE__)
      lis     r9,ha16(_PRE_Block)         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,lo16(_PRE_Block)(r9)
#else	
      lis     r9,PRE_Block@ha         /* Re-enable interrupt fiddling */
      li      r8,0
      stb     r8,PRE_Block@l(r9)
#endif /* NeXT || __APPLE__ */
      addi    r1,r1,FRAME_SIZE
      blr
#endif        /* __powerpc__ */
