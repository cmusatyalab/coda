/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Started with: xfs_dev.h,v 1.5 1999/01/31 19:46:36 assar Exp */

/* Modified by Philip A. Nelson, Aug 1999, for coda. */
/* $Id: $ */

#ifndef _cfs_dev_h
#define _cfs_dev_h

#include <cfs_common.h>

/*
 * Queues of cfs_links hold outbound messages and processes sleeping
 * for replies. The last field is used to return error to sleepers and
 * to keep record of memory to be deallocated when messages have been
 * delivered or dropped.
 */

struct cfs_link {
  struct cfs_link *prev, *next;
  union inputArgs *message;
  u_int      msgsize;		/* message size. */
  u_int	     outsize;		/* Output message size expected. */
  u_int      unique;
  u_int      error;
  kmutex_t   mutex;
  kcondvar_t cv;
  u_char     is_sleeping;
};  

/* Channel data */

struct cfs_channel {
  dev_info_t 	  *dip;
  struct cfs_link messageq;	/* Messages not yet read */
  struct cfs_link sleepq;	/* Waiting for reply message */
  u_int 	  nsequence;
  struct pollhead pollhead;
  union inputArgs *message_buffer;
  int 		  status;	/* See below. */
};

/* Channel status */
#define CHANNEL_OPENED	0x1

#define BUFFER_SIZE (1024*16)

/* Defines for waiting / no waiting */
#define UP_WAIT 1
#define UP_NO_WAIT 0

int cfs_devopen(dev_t *devp, int flags, int otyp, cred_t *credp);
int cfs_devclose(dev_t dev, int flags, int otyp, cred_t *credp);
int cfs_devread(dev_t dev, struct uio *uiop, cred_t *credp);
int cfs_devwrite(dev_t dev, struct uio *uiop, cred_t *credp);
int cfs_devioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
		 int *rvalp);
int cfs_chpoll(dev_t dev, short events, int anyyet,
	       short *reventsp, struct pollhead **phpp);

int cfs_dev_init(void);
int cfs_dev_fini(void);

int cfs_do_upcall (int minor, union inputArgs *message, u_int size,
		   u_int out_size, u_int waitreply);

int cfs_finish_upcall (union outputArgs *message, u_int size,
		       struct cfs_channel *chan);


#endif /* _cfs_dev_h */
