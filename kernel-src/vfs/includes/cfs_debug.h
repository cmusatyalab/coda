/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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

/* Started with: xfs_deb.h,v 1.4 1998/12/22 13:16:55 lha Exp $ */

/* Modified by Philip A. Nelson, Aug 1999, for coda. */
/* $Id: $ */

#ifndef _cfs_deb_h
#define _cfs_deb_h

/* Masks for the debug macro */
#define CDEBDEV		0x00000001	/* device handling */
#define CDEBDNC		0x00000002	/* downcalls */
#define CDEBUPC		0x00000004	/* downcalls */
#define CDEBDNLC	0x00000008	/* name cache */
#define CDEBNODE	0x00000010	/* xfs nodes */
#define CDEBVNOPS	0x00000020	/* vnode operations */
#define CDEBVFOPS	0x00000040	/* vfs operations */
#define CDEBLKM         0x00000080	/* LKM handling */
#define CDEBSYS	        0x00000100	/* syscalls */
#define CDEBMEM		0x00000200	/* memory allocation */
#define CDEBREADDIR     0x00000400      /* readdir */
#define CDEBLOCK	0x00000800	/* locking */
#define CDEBCACHE       0x00001000      /* Cache handeling */

extern int cfsdeb;

#ifdef DEBUG
#  define CFSDEB(mask, args) do { if (mask&cfsdeb) cmn_err args; } while (0)
#else
#  define CFSDEB(args) do { ; } while (0)
#endif

#define HAVE_CDEBDEV 1
#define HAVE_CDEBDWN 1
#define HAVE_CDEBDNLC 1
#define HAVE_CDEBNODE 1

#define HAVE_CDEBVNOPS 1
#define HAVE_CDEBVFOPS 1
#define HAVE_CDEBLKM 1
#define HAVE_CDEBSYS 1

#endif /* _cfs_deb_h */
