/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/* Started with:  xfs_node.h,v 1.5 1998/12/22 13:16:55 lha Exp  */

/* Modified by Philip A. Nelson, Aug 1999, for coda. */
/* $Id: $ */

#ifndef _cfs_node_h
#define _cfs_node_h

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vnode.h>

struct cfs_node {
  struct vnode 	vn;		/* The vnode itself! */
  struct vattr 	attr;		/* Attributes */
  u_int 	flags;		/* Flags ... see below */
  ViceFid	fid;		/* file handle */
  struct vnode	*ovp;		/* open vnode pointer */
  u_short	ocount;		/* count of openers */
  u_short	owrite;		/* count of open for write */
  char		*symlink;	/* pointer to symbolic link */
  u_short	symlen;		/* length of symbolic link */
  dev_t		device;		/* associated vnode device */
  ino_t		inode;		/* associated vnode inode */

  struct cfs_node *next;
};

/* flags */
#define C_VATTR         0x01    /* Validity of vattr in the cnode */
#define C_SYMLINK       0x02    /* Validity of symlink pointer in the Code */
#define C_WANTED        0x08    /* Set if lock wanted */
#define C_LOCKED        0x10    /* Set if lock held */
#define C_UNMOUNTING    0X20    /* Set if unmounting */

#define DATA_FROM_VNODE(vp) ((struct vnode *) (vp)->v_data)
#define DATA_FROM_CNODE(xp) DATA_FROM_VNODE(CNODE_TO_VNODE(xp))

#define CNODE_TO_VNODE(xp) (&((xp)->vn))
#define VNODE_TO_CNODE(vp) ((struct cfs_node *) vp)

#endif /* _cfs_node_h */
