/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

/* Started with: xfs_fs.h,v 1.4 1998/12/22 13:16:55 lha Exp */

/* Modified by Philip A. Nelson, Aug 1999, for coda. */

#ifndef _cfs_h
#define _cfs_h

#include <cfs_common.h>
#include <cfs_node.h>

/*
 * Filesystem struct.
 */
struct cfs {
  u_int 	status;		/* Inited, opened or mounted */
  struct vfs 	*vfsp;
  struct cfs_node *root;
  u_int 	nnodes;
  int 		minor;		/* Which device to use for upcall. */

  struct cfs_node *nodes;		/* replace with hash table */
};

/* Filesystem status codes. */
#define CFS_MOUNTED	0x1

#define VFS_TO_CFS(v)      ((struct cfs *) ((v)->vfs_data))
#define CFS_TO_VFS(x)      ((x)->vfsp)

#define CFS_FROM_VNODE(vp) VFS_TO_CFS((vp)->v_vfsp)
#define CFS_FROM_CNODE(xp) CFS_FROM_VNODE(CNODE_TO_VNODE(xp))

extern struct cfs cfs[];

extern struct vnodeops cfs_vnodeops;

void free_all_cfs_nodes (struct cfs *cfsp);
void free_cfs_node (struct cfs_node *);
struct cfs_node *cfs_node_find (struct cfs *, ViceFid *);
struct cfs_node *new_cfs_node (struct cfs *, ViceFid *, vtype_t);

#if 0

int 
cfs_dnlc_enter (struct vnode *, char *, struct vnode *);

struct vnode *
cfs_dnlc_lookup (struct vnode *, char *);

void cfs_dnlc_purge (void);

void
cfs_dnlc_remove(vnode_t *dvp, char *name);

void
cfs_attr2vattr (const struct cfs_attr *xa, struct vattr *va);

void
vattr2cfs_attr (const struct vattr *va, struct cfs_attr *xa);

int cfs_has_pag(const struct cfs_node *xn, pag_t);

#endif

#endif /* _cfs_h */
