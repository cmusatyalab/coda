#ifndef _BLURB_
#define _BLURB_
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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/rvmtesting.cc,v 1.2 1997/01/07 18:41:47 rvb Exp";
#endif /*_BLURB_*/





#ifdef	RVMTESTING
#ifdef __cplusplus
extern "C" {
#endif __cplusplus
    
#include <stdio.h>
#ifdef __MACH__

#include <sysent.h>
#include <cthreads.h>
#endif /* __MACH__ */
    
#ifdef __cplusplus
}
#endif __cplusplus

#include "util.h"

/* Need to assign this in code you are debugging! */
unsigned long *ClobberAddress = 0;

void protect_page(int x)
{
    assert(x != 0);
    kern_return_t code = vm_protect(task_self(), (vm_address_t)x,
				    sizeof(int), FALSE,
				    VM_PROT_READ | VM_PROT_EXECUTE);
    if (code != KERN_SUCCESS) {
	LogMsg(0, 0, stdout, "vm_protect(%x, %d, rwx) failed (%d)",
	       x, sizeof(int), code);
	assert(0);
    }
}    

void unprotect_page(int x)
{
    assert(x != 0);
    kern_return_t code = vm_protect(task_self(), (vm_address_t)x,
				    sizeof(int), FALSE,
				    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
    if (code != KERN_SUCCESS) {
	LogMsg(0, 0, stdout, "vm_unprotect(%x, %d, rwx) failed (%d)",
	       x, sizeof(int), code);
	assert(0);
    }
}    
/*
 * Here's the plan:
 *  	1. Make sure we have a store (or a branch with a store in the delay slot)
 * 	2. Check if the store is writing the address in ClobberAddress.
 *	3. If so, zombie. If not, emulate the instruction (Unprotecting the page)
 *	4. Determine the next PC value and set the pc to go there upon return.
 */

unsigned long savedInstruction = 0;
#define BADINSTRUCTION 0xff000000	/* No legal opcode 0xff */
extern void zombie(int sig, int code, struct sigcontext *scp);

#define LANGUAGE_C 1
#include <machine/inst.h>
#undef LANGUAGE_C

int GPR(struct sigcontext *scp, int x) /* Get value of General Purpose Register */
{
    if (x < 32 && x >= 0)
	return scp->sc_regs[x];
    zombie(11, 11, scp);
    return 0;				/* Stupid but true, C++ requires this */
}    

/* Decode the instruction if it's a branch or a jump, otherwise it'd better
 * be a store.
 */

PRIVATE int DCSDebug = 0;

PRIVATE int getNextPc(struct sigcontext *scp)
{
    union mips_instruction *pc = (union mips_instruction *)scp->sc_pc;
    unsigned int temp = 0, cond = 0;
    
    switch (pc->j_format.opcode) {	/* All opcodes are the first 6 bits */

      case spec_op : {  /* Either it's a jr or a jalr or we've got a problem. */
	  switch(pc->r_format.func) {

	      /* JumpRs are performed by jumping to the address contained in rs */
	    case jalr_op:
	      /* JALR needs to set rd to pc + 2 */
	      assert(pc->r_format.rd < 32 && pc->r_format.rd >= 0);
	      scp->sc_regs[pc->r_format.rd] = (int)pc + 8;
	    case jr_op:
	      temp = GPR(scp, pc->r_format.rs);
	      LogMsg(5, DCSDebug, stdout, "JumpR to %08#x", temp);
	      return temp;

	    default:
	      LogMsg(5, DCSDebug, stdout, "SPECIAL op error! Not a jump!");
	      assert(0);	
	  }	      
      }

	/* Jumps: The address is formed by concatenating the 4 high bits
	 * from the PC with the target value in the instruction shifted
	 * left 4 bits (t * 4)
	 */
   
      case jal_op:	/* "and link" part stores address to which to return */
	temp = (int)pc;
	scp->sc_regs[31] = temp + 8;
	
      case j_op: 	/* j and jal -- j-type instructions. */
	  temp = ((int)pc & 0xf0000000) | (pc->j_format.target << 2);
	  LogMsg(5, DCSDebug, stdout, "Jump to %08#x", temp);
	  return temp;

	/* Branches: The address is formed by signed extending the offset
	 * from the instruction, multiplying it by four, and adding it to the
	 * PC of the instruction in the delay slot. Since sizeof(pc) == 4,
	 * pc += temp indirectly multiplies temp by 4 for us!.
	 */
      case bcond_op: {	/* bgez, bgezal, bltz, bltzal: I-type instructions */
	  switch(pc->i_format.rt) {
	    case bgezal_op: /* "and link" needs to set reg[31] to pc + 2 */
	      scp->sc_regs[31] = (int)pc + 8;
	    case bgez_op:
	      cond = !(GPR(scp, pc->i_format.rs) & 0x80000000);
	      break;

	    case bltzal_op: /* "and link" needs to set 31 to pc + 2 */
	      scp->sc_regs[31] = (int)pc + 8;
	    case bltz_op:
	      cond = GPR(scp, pc->i_format.rs) & 0x80000000;
	      break;

	    default:
	      assert(0);
	  }
	  /* Sign extend the offset */
	  temp = (pc->i_format.simmediate << 16) >> 16;

	  if (cond) 
	      pc += temp;
	  else
	      pc ++;
	  pc ++; /* Branch refers to address of delay slot */
	  LogMsg(5, DCSDebug, stdout, "Bcond to %08#x", pc);
	  return (int)pc;
      }	/* bcond */
	  
      case beq_op: {				/* I-type instruction */
	  /* Sign extend the offset */
	  temp = (pc->i_format.simmediate << 16) >> 16;
	  if (GPR(scp, pc->i_format.rs) == GPR(scp, pc->i_format.rt))
	      pc += temp;
	  else
	      pc ++;
	  pc ++; /* Branch refers to address of delay slot */
	  LogMsg(5, DCSDebug, stdout, "Beq to %08#x", pc);
	  return (int)pc;
      }
	  
      case bne_op: {				/* I-type instruction */
	  /* Sign extend the offset */
	  temp = (pc->i_format.simmediate << 16) >> 16;
	  if (GPR(scp, pc->i_format.rs) != GPR(scp, pc->i_format.rt))
	      pc += temp;
	  else
	      pc ++;
	  pc ++; /* Branch refers to address of delay slot */
	  
	  LogMsg(5, DCSDebug, stdout, "Bne to %08#x", pc);
	  return (int)pc;
      }

      case blez_op: {				/* I-type instruction */
	  /* Sign extend the offset */
	  temp = (pc->i_format.simmediate << 16) >> 16;
	  if ((GPR(scp, pc->i_format.rs) & 0x80000000) || (GPR(scp, pc->i_format.rs) == 0))
	      pc += temp;
	  else
	      pc ++;
	  pc ++; /* Branch refers to address of delay slot */
	  LogMsg(5, DCSDebug, stdout, "Blez to %08#x", pc);
	  return (int)pc;
      }
	  
      case bgtz_op: {				/* I-type instruction */	
	  /* Sign extend the offset */
	  temp = (pc->i_format.simmediate << 16) >> 16;
	  if (!(GPR(scp, pc->i_format.rs) & 0x80000000) || (GPR(scp, pc->i_format.rs) == 0))
	      pc += temp;
	  else
	      pc ++;
	  pc ++; /* Branch refers to address of delay slot */
	  LogMsg(5, DCSDebug, stdout, "Bgtz to %08#x", pc);
	  return (int)pc;
      }

	/* The instruction after a store is 4 bytes away. */
      case sb_op:
      case sh_op:
      case sw_op:
      case swl_op:
      case swr_op:
	  pc ++;
	  return (int)pc;

      default:
	  assert(0);
    }
}    

#define StoreInLoadDelay(x) 	/* Check if instruction is a branch or jump */  \
    (((((union mips_instruction *) x)->j_format.opcode > 0x0) && \
      (((union mips_instruction *) x)->j_format.opcode < 0x8)) ||\
     ((((union mips_instruction *) x)->j_format.opcode == 0x0) && \
      ((((union mips_instruction *) x)->r_format.func == jalr_op) || \
       (((union mips_instruction *) x)->r_format.func == jr_op))))

extern camlog_fd;
    
void my_sigBus(int sig, int code, struct sigcontext *scp) {
    
    LogMsg(3, DCSDebug, stdout,  "****** FILE SERVER INTERRUPTED BY SIGNAL %d ******", sig);

    LogMsg(3, DCSDebug, stdout, "sig=%d, code=%d pc=%08#x *pc=%08#x", sig, code,
	   scp->sc_pc, *(int *)scp->sc_pc);

    /* First, if they're writing to the address in question, just zombie. */

    while (DCSDebug > 6) ;
    
    union mips_instruction *storePC = (union mips_instruction *)scp->sc_pc;
    
    if (StoreInLoadDelay(storePC)) 
	storePC++;

    switch (storePC->i_format.opcode) {
	int wrAddr;
	int value;
      case sb_op:
      case sh_op:
      case sw_op:
      case swl_op:
      case swr_op:	/* It's a store instruction, I-type */
	/* I think the next line is incorrect -bnoble*/
	/* wrAddr = ((storePC->i_format.simmediate << 16) >> 14) & 0xfffffffc; */
	/* get sign extended offset to the base register */
	wrAddr = ((storePC->i_format.simmediate << 16) >> 16);
	/* Add the contents of the base register */
	wrAddr += GPR(scp, storePC->i_format.rs);
	/* Zombie if the two low order bits are nonzero, and we
	   are a sw_op.  Zombie if the low order bit is nonzero,
	   and we are a sh_op */
	if (storePC->i_format.opcode == sw_op)
	{
	    if ((wrAddr & 0x3) != 0)
		zombie(sig,code,scp);
	}
	else 
	{
	    if(storePC->i_format.opcode == sh_op)
	    {
		if ((wrAddr & 0x1) != 0)
		{
		    zombie(sig,code,scp);
		}
	    }
	}
	/* Check to see if they're writing to the bad address (must be aligned) */
	LogMsg(1, DCSDebug, stdout, "CHECK: wrAddr %08#x Clobber %08#x",
	       wrAddr, ClobberAddress);
	/* (I don't believe this either, but I don't know how to fix it
	   -bnoble */
	if ((wrAddr & 0xfffffffc) == (int)ClobberAddress) { 
	    LogMsg(0, 0, stdout, "ACKK! Someone is writing to our address %08#x",
		   ClobberAddress);
	    zombie(sig, code, scp);
	}

	/* Emulate the instruction */
	unprotect_page(wrAddr);
	value = GPR(scp, storePC->i_format.rt);
	if (storePC->i_format.opcode == sb_op) {
	    char *dest = (char *)wrAddr;
	    char *src = (char *)&value;
	    *dest = src[0];
	} else if (storePC->i_format.opcode == sh_op) {
	    short *ptr = (short *)wrAddr;
	    short *src = (short *)&value;
	    *ptr = src[0];
	} else if (storePC->i_format.opcode == sw_op) {
	    int *ptr = (int *)wrAddr;
	    *ptr = value;
	} else { /* swl or swr, It looks hard/wierd so I'll skip it for now */
	    LogMsg(0, 0, stdout, "ACKK! Got a swl or swr so I'm aborting!\n");
	    zombie(sig, code, scp);
	}
	protect_page(wrAddr);
	
	break;
	
      default:		/* How did it get a sigbus here? */
	zombie(sig, code, scp);
    }
    
    /* Update the pc to it's new value? */
    scp->sc_pc = getNextPc(scp);
    

}
#endif  RVMTESTING
