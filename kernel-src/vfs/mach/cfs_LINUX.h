#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 3.1

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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/kernel-src/vfs/mach/Attic/cfs_LINUX.h,v 4.1 1997/01/08 21:53:24 rvb Exp $";
#endif /*_BLURB_*/


/* A include file to provide Linux support for SETS */
#ifndef _LINUX_H
#define _LINUX_H 1

/* Linux source tree is different than Mach's, so include magic is different. */
#include <linux/limits.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/wait.h>		
#include <linux/fs.h>
#include <linux/types.h>

/* Linux's version of printf */
#define printf printk

/* Number of minor devices (e.g. odyssey tomes) allowed */
#define NVCFS 5
#define ODYSSEY_MAJOR 19	/* Why the hell not? */

/* Linux doesn't support Sun's VFS, a vnode is just an inode anyway */
/* XXX */
/* Right now this is mostly a hack. I really need to think this through if
 * coda is going to be ported to linux. odyssey doesn't need any of this
 * for the moment. -- DCS 8/24/94
 */
#define vnode inode
#define v_count i_count
#define v_flag  i_flags
#define v_next  i_next
#define ITOV(ip)
#define VTOI(vp)
#define vm_info_init(a) 
#define VM_INFO_NULL NULL
#define inode_uncache_try(a) ETXTBSY
    
#ifdef NOTDEF
#define iget(a, b, c) a
#define v_vm_info i_mmap	/* Something of a reach, but seems reasonable for now=) */
#define vm_info vm_area_struct  /* Something of a reach, but seems reasonable for now=) */
#define	VTEXT		0	/* This is just a hack to get it compiled... */

struct vfs { /* What should this be? Possibly operations? */
    int foobar;
};

struct vfsops {
	int	(*vfs_mount)();		/* mount file system */
	int	(*vfs_unmount)();	/* unmount file system */
	int	(*vfs_root)();		/* get root vnode */
	int	(*vfs_statfs)();	/* get fs statistics */
	int	(*vfs_sync)();		/* flush fs buffers */
	int	(*vfs_vget)();		/* get vnode from fid */
};
#endif NOTDEF
/* XXX */

/* I assume this will work;-) */
#define VN_HOLD(ip) ((struct inode *)ip)->i_count++; 
#define VN_RELE(vp) iput((struct inode *)vp);


/*
 * Vnode attributes.  A field value of -1
 * represents a field whose value is unavailable
 * (getattr) or which is not to be changed (setattr).
 */
struct vattr {
	u_short		va_mode;	/* files access mode and type */
	short		va_uid;		/* owner user id */
	short		va_gid;		/* owner group id */
	long		va_fsid;	/* file system id (dev for now) */
	long		va_nodeid;	/* node id */
	short		va_nlink;	/* number of references to file */
	u_long		va_size;	/* file size in bytes (quad?) */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timeval	va_atime;	/* time of last access */
	struct timeval	va_mtime;	/* time of last modification */
	struct timeval	va_ctime;	/* time file ``created */
	dev_t		va_rdev;	/* device the file represents */
	long		va_blocks;	/* kbytes of disk space held by file */
};

#include <linux/dirent.h>
#define direct dirent

#define DIR_ADVANCE(dp) dp = NULL /* In linux, only 1 entry returned at a time. */

#define DIR_PAGE_SIZE sizeof(struct dirent)
#define GET_DIR(dirname, dir, error) \
    { extern int dir_namei(const char * pathname, int * namelen, const char ** name,	struct inode * base, struct inode ** res_inode); \
      int get_dir_len; \
      const char *get_dir_name; \
      error = dir_namei(dirname, &get_dir_len, &get_dir_name, 0, dir); \
    }

#define SET_OPEN_OBJECT(name, flag, vp, error) \
    { const char *duh = name; \
      error = open_namei(duh, flag, 0, vp, 0); \
    } 

#include <linux/string.h>
#define bzero(s, size) memset(s, 0, size);
#define bcopy(src, dst, len) memcpy(dst, src, len);

/* Similarly, it uses different names for these. fs is the user segment */
/* #define copyinstr(from, to, len, nbytes) memcpy_fromfs(to, from, len) */

#include <asm/segment.h>
int copyFromUserSpace(const char *from, char *to, int len);
#define copyToUserSpace(from, to, len) memcpy_tofs(to, from, len)

/* Not sure what's the correct definition. See comment in fs/open.c at line 362 */
#define FREAD 0

/* Managing file descriptors */
extern int closeOpenFile(struct file *);
#define CLOSE_FILE(fp) closeOpenFile((struct file *)fp)

#define GET_FP(thing, fd)	thing = (char *)current->filp[fd]
    
static inline void RELEASE_FP(char **thing)
{
    struct file *filp = (struct file *) *thing;
    *thing = (char *)filp->f_inode;
    CLOSE_FILE(filp);
}

/* per-process definitions. A hack to allow the sets worker thread to run on
 * behalf of the caller thread.
 */

#define Process_setStuff current->setStuff
#define Process_cdir  	 current->pwd
#define Process_root	 current->root
#define Process_pid 	 current->pid	
#define Process_pgid 	 current->pgrp

/* Linux in-lines protection stuff, no struct identity */
/* Should this be os specific? */
struct CodaCred {
    uid_t uid, euid, suid;	    /* Real, set, and effective uid for caller*/
    gid_t gid, egid, sgid;	    /* Real, set, and effective uid for caller*/
    gid_t groups[NGROUPS];	    /* Group membership for caller */
};
#define identity CodaCred
#define FREE_ID(id)	MYFREE((id), sizeof(struct identity))

#define SAVE_ID(id) \
  { \
      int i; \
      MYALLOC((id), struct identity *, sizeof(struct identity)); \
      (id)->uid = current->uid; (id)->euid = current->euid; (id)->suid = current->suid;\
      (id)->gid = current->gid; (id)->egid = current->egid; (id)->sgid = current->sgid; \
      for (i = 0; i < NGROUPS; i++) (id)->groups[i] = current->groups[i]; \
  }

#define CHANGE_ID(id) \
  { \
      int i; \
      current->uid = (id)->uid; current->euid = (id)->euid; current->suid = (id)->suid;\
      current->gid = (id)->gid; current->egid = (id)->egid; current->sgid = (id)->sgid; \
      for (i = 0; i < NGROUPS; i++) (id)->groups[i] = current->groups[i]; \
  }

#define FREE_MY_IDENTITY()

#define COPY_CRED_TO_CODACRED(cred, ident) \
  { \
      int i; \
      (cred)->uid = current->uid; (cred)->euid = current->euid; (cred)->suid = current->suid; \
      (cred)->gid = current->gid; (cred)->egid = current->egid; (cred)->sgid = current->sgid; \
      for (i = 0; i < NGROUPS; i++) (in)->cred.groups[i] = (ident)->id_groups[i]; \
  }

/* I need the operation to run on behalf of whomever spawned the msg, so 
 * use its curdirectory and identity.
 */
#define BECOME_CALLER(msg) \
    /*HACK*/ CHANGE_ID(msg->set->identity); \
    /*HACK*/ Process_cdir = msg->set->curdir; \
    /*HACK*/ Process_root = msg->set->rootdir; 

#define BECOME_MYSELF(mycdir)

/* Maximal pathname length */
#define MAXPATHLEN PATH_MAX

/* I need os-specific macros for sleep/wakeup. */
#define SELPROC struct wait_queue * /* pointer? */
#define SELWAKEUP(proc) if (proc) wake_up_interruptible(&(SELPROC)(proc));
#define WAKEUP(cond) wake_up_interruptible((cond));
#define SLEEP(cond, prio)  interruptible_sleep_on((cond))
#define PZERO 1	/* sleep priorities not used in linux */

/* Linux kalloc is slightly different than mach's */
#define KALLOC(size) kmalloc(size, GFP_KERNEL)

/* Help find malloc/free bugs */
#define MYALLOC(ptr, cast, size) \
    { \
	ptr = (cast)kmalloc((int)(size), GFP_KERNEL); \
	if (ptr == 0) panic("kmalloc returns 0 at %s:%d\n", __FILE__, __LINE__); \
        else  \
	    DEBUG(ALLOC, printf("%s:%d kalloc 0x%x\n",__FILE__, __LINE__, (int)(ptr));)\
    }

#define MYFREE(ptr, size) \
    { \
	DEBUG(ALLOC, printf("%s:%d Free 0x%x\n", __FILE__, __LINE__, (int)ptr);); \
	kfree_s(ptr, size); \
    }

/* As in mach, have all string objects be 1024 in size. This may be a little
 * wasteful in space, but will probably save a little time. Could easily
 * do the opposite if necessary.
 */
#define STRALLOC(ptr) \
    { \
	ptr = (char *)kmalloc(PATH_MAX + 1, GFP_KERNEL); \
	if (ptr == 0) panic("stralloc returns 0 at %s:%d\n", __FILE__, __LINE__); \
	else \
	  DEBUG(ALLOC, printf("%s:%d Str allocate %x\n", __FILE__, __LINE__, (int)ptr););\
    }

#define STRFREE(ptr) \
    { \
	DEBUG(ALLOC, printf("%s:%d Str Free 0x%x\n",__FILE__,__LINE__, (int)ptr););\
	kfree_s((char *)ptr, PATH_MAX+1); \
    }

/*
 * thread synchronization support on linux
 */

#define CONDITION struct wait_queue *

/* MUTEX is badly defined in linux/wait.h */
/*
 * QUESTION? Would it be easier to just use spin locks and assume the
 * critical section is small/short? No, duhh. we're on a uniprocessor, so
 * if a lock were held and we spun, the lock would never get released.
 * It's probably the case that this situation will never happen anyway...
 */

struct mymutex {
    int lock_data;
    CONDITION wait;
};

#ifdef MUTEX
#undef MUTEX
#define MUTEX struct mymutex
#define MUTEX_INIT(m) (m)->lock_data = 0; (m)->wait = NULL;
#endif MUTEX

#ifndef DEBUGCTHREAD

#define cthread_yield() { printf("Yielding %s:%d\n", __FILE__, __LINE__); }

#define condition_signal(cond) \
    { \
       DEBUG(THREAD,printf("Signaling condition 0x%x %s:%d\n", (int)(cond),__FILE__,__LINE__););\
       wake_up_interruptible((cond)); \
    }

#define mutex_unlock(m)\
    { \
	DEBUG(THREAD,printf("mutex_unlock (0x%x)%s:%d...", (int)(m), __FILE__, __LINE__);); \
	simple_unlock((m)); \
        DEBUG(THREAD,printf("done\n");); \
    }

#define mutex_lock(m)\
    { \
	DEBUG(THREAD,printf("mutex_lock (0x%x)%s:%d...", (int)(m), __FILE__, __LINE__);); \
	simple_lock((m)); \
        DEBUG(THREAD,printf("done\n");); \
    }

#define condition_wait(cond, lock) \
    { \
       DEBUG(THREAD,printf("Waiting on condition 0x%x %s:%d\n", (int)(cond),__FILE__,__LINE__););\
       sets_condition_wait(cond, lock); \
       DEBUG(THREAD,printf("Recieved signal on condition 0x%x %s:%d\n", (int)(cond),__FILE__,__LINE__););\
    } 
    
#else DEBUGCTHREAD
#define mutex_lock(m)			simple_lock((m));
#define mutex_unlock(m)			simple_unlock((m));


#define cthread_yield() { DEBUG(THREAD,printf("Yielding %s:%d\n", __FILE__, __LINE__);); }

#define condition_signal(cond) \
    { \
       wake_up_interruptible((cond)); \
    }

#define condition_wait(cond, lock) sets_condition_wait(cond,lock)

#endif  DEBUGCTHREAD

extern void sets_condition_wait(CONDITION *cond, MUTEX *lock);


#ifdef assert
#undef assert
#endif assert

#define assert(cond) 							\
    if (!(cond)) {    							\
	printf("Assert at line \"%s\", line %d\n", __FILE__, __LINE__); \
	for (;;) ; \
    }


/*
 * Mach provides some nice queueing funtions, so I'll redefine them here.
 */
struct queue_entry {
        struct queue_entry      *next;          /* next element */
        struct queue_entry      *prev;          /* previous element */
};

typedef struct queue_entry      queue_chain_t;
typedef struct queue_entry      *queue_entry_t;

#define queue_first(queue) (queue)->next
#define queue_end(queue, elem)   ((queue) == (elem))
#define queue_next(elem)   (elem)->next
#define queue_init(queue) (queue)->next = (queue)->prev = (queue);
#define queue_empty(queue) ((queue) == (queue)->next)

#define enqueue(queue, elem) \
    { \
	 (elem)->next = (queue)->next; \
	 (elem)->prev = queue; \
	 (elem)->next->prev = elem; \
	 (queue)->next = elem; \
    }
queue_entry_t dequeue(queue_chain_t *queue);

/* HACK because linux insists on writing dir entries to user space. */

#define CHECK_USER_BUFFER(buf, size) \
  { \
     int tmp; \
     if (bufsize < DIR_PAGE_SIZE) \
	return -EINVAL; \
    tmp = verify_area(VERIFY_WRITE, buf, sizeof(size)); \
    if (tmp) return tmp; \
  }

/* So stupid. these includes need the crap defined here. */
/* #include <linux/cnode.h> */
/* #include <linux/cfs_opstats.h> */
#endif _LINUX_H

