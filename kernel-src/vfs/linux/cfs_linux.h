/* 
 * Linux support for Coda
 * Peter Braam, 9/1996
 *
 */

#ifndef _LINUX_CODA_FS
#define _LINUX_CODA_FS


/*
 * kernel includes are clearly platform dependent 
 * 
 * however, Venus builds its own VFS interface, modeled on NetBSD 
 */
#ifdef KERNEL
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/sched.h> 
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/wait.h>		
#include <linux/types.h>
#include <linux/fs.h>
#else 
#include <venus_vnode.h>
#endif KERNEL


/* directory entries; should go somewhere else XXX */
#ifdef KERNEL
#ifdef LINUX
#define u_int32_t   unsigned int
#define u_int16_t   unsigned short
#define u_int8_t    char
#endif

struct venus_dirent {
        u_int32_t d_fileno;             /* file number of entry */
        u_int16_t d_reclen;             /* length of this record */
        u_int8_t  d_type;               /* file type, see below */
        u_int8_t  d_namlen;             /* length of string in d_name */
#ifdef _POSIX_SOURCE
        char    d_name[255 + 1];        /* name must be no longer than this */
#else
#define MAXNAMLEN       255
        char    d_name[MAXNAMLEN + 1];  /* name must be no longer than this */
#endif
};

#endif kernel


/* debugging aids */

#define panic printk


#define D_VENUSRET  1   /* print results returned by Venus */ 
#define D_ENTRY     2   /* print entry and exit into procedure */
#define D_MALLOC    4   /* print malloc, de-alloc information */

#define myprintf(ARG)  printk ARG

#define COMPLAIN_BITTERLY(OP, FID)                             \
do {                                                           \
    myprintf(("Acck! %s on dead vnode %lx.%lx.%lx attempted ", \
           #OP, (FID).Volume, (FID).Vnode, (FID).Unique));     \
    myprintf(("by Process %d (parent %d),",                    \
	   GLOBAL_PROC->pid, GLOBAL_PROC->p_pptr->pid));       \
    myprintf((" in %s, at %d.\n", __FUNCTION__, __LINE__));    \
} while (0) ;


#define DEBUG(format, a...)                                       \
  do {                                                            \
  if (coda_debug) {                                               \
    printk("%s, line: %d ",  __FUNCTION__, __LINE__);             \
    printk(format, ## a); }                                       \
} while (0) ;                            
 
#define CDEBUG(mask, format, a...)                                \
  do {                                                            \
  if (coda_debug & mask) {                                        \
    printk("(%s,l. %d): ",  __FUNCTION__, __LINE__);              \
    printk(format, ## a); }                                       \
} while (0) ;                            

#define ENTRY    \
    if(coda_debug & D_ENTRY) printk("Process %d entered %s\n",current->pid,__FUNCTION__)

#define EXIT    \
    if(coda_debug & D_ENTRY) printk("Process %d leaving %s\n",current->pid,__FUNCTION__)


/* 
 * process and credentials stuff 
 */

#define GLOBAL_PROC current
#define GLOBAL_CRED 

/*
 * Mach flags/types that are missing 
 */

enum vcexcl	{ NONEXCL, EXCL};		/* (non)excl create (create) */

#define FREAD           0x0001
#define FWRITE       (O_WRONLY | O_RDWR)
#define FTRUNC       O_TRUNC
#define FCREAT       O_CREAT
#define FEXCL        O_EXCL


/* 
 *      Linux vfs stuff 
 */

#define VFS_T           struct coda_sb_info
#define VFS_DATA        mnt_sb 

/* Linux doesn't support Sun's VFS, a vnode is just an inode anyway */
/* these macros should only be called for Linux in core inodes */

/* inode to coda_inode */
#define ITOCI(the_inode) (struct coda_inode *)((the_inode)->u.generic_ip)
/* inode to cnode */
#define ITOC(the_inode)  &((ITOCI(the_inode))->ci_cnode)
/* cnode to inode */
#define CTOI(the_cnode)  ((the_cnode)->c_vnode)


#define panic printk
#define CHECK_CNODE(c)                                                \
do {                                                                  \
  struct cnode *cnode = (c);                                          \
  if (!cnode)                                                         \
    panic ("%s(%d): cnode is null\n", __FUNCTION__, __LINE__);        \
  if (cnode->c_magic != CODA_CNODE_MAGIC)                             \
    panic ("%s(%d): cnode magic wrong\n", __FUNCTION__, __LINE__);    \
  if (!cnode->c_vnode)                                                \
    panic ("%s(%d): cnode has null inode\n", __FUNCTION__, __LINE__); \
} while (0);

#define CN_INIT(CP)        memset((CP), 0, (int)sizeof(struct cnode))
#define CI_INIT(CI)        bzero((CI), 0, (int)sizeof(struct coda_inode))

typedef struct inode *C_VNODE_T;
#define LINKS  struct cnode *c_next     /* needed in cnode.h */
#ifdef KERNEL
#define v_count i_count
#define v_flag  i_flags
#define v_next  i_next
#endif
#define ITOV(ip) 
#define VTOI(vp) 
#define vm_info_init(a) 
#define VM_INFO_NULL NULL
#define inode_uncache_try(a) ETXTBSY

#ifdef KERNEL
#define CTOV(cp)        CTOI(cp)
#define VTOC(vp)        ITOC(vp)
#else 
#define    CTOV(cp)        ((struct vnode *)((cp)->c_vnode))
#define    VTOC(vp)        ((struct cnode *)(vp)->v_data)
#endif KERNEL
#define CNODE_NEXT(cp)  ((cp)->c_next)

/* ioctl stuff */
/* this needs to be sorted out XXXX */ 
#ifdef LINUX
#define IOCPARM_MASK 0x0000ffff
#endif 


/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 */

struct cfid {
    u_short	cfid_len;
    u_short     padding;
    ViceFid	cfid_fid;
};

#define MAXSYMLINKS 16
#define VN_TYPE(vp)        (vp)->v_type


#define CODA_ALLOC(ptr, cast, size)                                       \
do {                                                                      \
    if (size < 3000) {                                                    \
        ptr = (cast)kmalloc((unsigned long) size, GFP_KERNEL);            \
                CDEBUG(D_MALLOC, "alloced: %x at %x.\n", (int) size, (int) ptr);\
     }  else                                                              \
        ptr = (cast)vmalloc((unsigned long) size);                        \
    if (ptr == 0) {                                                       \
        panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
    memset( ptr, 0, size );                                                   \
} while (0)


#define CODA_FREE(ptr, size) do {if (size < 3000) { kfree_s((ptr), size); CDEBUG(D_MALLOC, "de-alloced: %x at %x.\n", (int) size, (int) ptr); } else vfree((ptr));} while (0)



#ifdef KERNEL
enum vtype      { VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };
#define VFMT		S_IFMT
#define VCHR		S_IFCHR
#define VDIR		S_IFDIR
#define VBLK		S_IFBLK
#define VREG		S_IFREG
#define VLNK		S_IFLNK
#define VSOCK		S_IFSOCK
#define VSUID		S_ISUID
#define VSGID		S_ISGID
#define VSVTX		S_ISVTX
#define VREAD		S_IREAD
#define VWRITE		S_IWRITE
#define VEXEC		S_IEXEC

#define VTEXT           0  /* XXX what on earth is going on? */

#endif KERNEL

/* I assume this will work;-) */
#define VN_HOLD(ip) ((struct inode *)ip)->i_count++; 
#define VN_RELE(vp) iput((struct inode *)vp);



/*
 * Vnode attributes.  A field value of -1
 * represents a field whose value is unavailable
 * (getattr) or which is not to be changed (setattr).

 *  XXXX this is all very different under LINUX
 *  there we have iattr's (see fs.h).
 *  However, the iattr's define very little of the inode,
 *  perhaps too little:
 */

#ifndef _VUID_T_
#define _VUID_T_
typedef unsigned int vuid_t;
typedef unsigned int vgid_t;
#endif _VUID_T

#ifdef KERNEL

/* Venus - Kernel communication macros */

#define HANDLE_ERROR( errorp, out, label)  \
if ( coda_upcallerror( errorp, out, __FUNCTION__) ) goto label ; 




#define DIRBLKSIZ 1024  /*  from NetBSD 1.2 dirent.h */


#if 0
struct timespec {
        long       ts_sec;
        long       ts_nsec;
};
#endif

#define u_quad_t u_long

#ifndef _VUID_T_
#define _VUID_T_
typedef unsigned int vuid_t;
typedef unsigned int vgid_t;
#endif _VUID_T_

struct vattr {
        enum vtype      va_type;        /* vnode type (for create) */
        u_short         va_mode;        /* files access mode and type */
        short           va_nlink;       /* number of references to file */
        vuid_t           va_uid;         /* owner user id */
        vgid_t           va_gid;         /* owner group id */
        long            va_fsid;        /* file system id (dev for now) */
        long            va_fileid;      /* file id */
        u_quad_t        va_size;        /* file size in bytes */
        long            va_blocksize;   /* blocksize preferred for i/o */
        struct timespec va_atime;       /* time of last access */
        struct timespec va_mtime;       /* time of last modification */
        struct timespec va_ctime;       /* time file changed */
        u_long          va_gen;         /* generation number of file */
        u_long          va_flags;       /* flags defined for file */
        dev_t           va_rdev;        /* device the special file represents */
        u_quad_t        va_bytes;       /* bytes of disk space held by file */
        u_quad_t        va_filerev;     /* file modification number */
        u_int           va_vaflags;     /* operations flags, see below */
        long            va_spare;       /* remain quad aligned */
};


#endif

#define Process_pid    current->pid
#define Process_pgid   current->gid

/* Linux in-lines protection stuff, no struct identity */
/* Should this be os specific? */
#define NGROUPS 32
struct CodaCred {
    vuid_t cr_uid, cr_euid, cr_suid, cr_fsuid; /* Real, efftve, set, fs uid*/
    vgid_t cr_gid, cr_egid, cr_sgid, cr_fsgid; /* same for groups */
    vgid_t cr_groups[NGROUPS];	      /* Group membership for caller */
};
#define ucred CodaCred


/* I need the operation to run on behalf of whomever spawned the msg, so 
 * use its curdirectory and identity.
 */
#if 0
#define BECOME_CALLER(msg) \
    /*HACK*/ CHANGE_ID(msg->set->identity); \
    /*HACK*/ Process_cdir = msg->set->curdir; \
    /*HACK*/ Process_root = msg->set->rootdir; 

#define BECOME_MYSELF(mycdir)
#endif 
/* Maximal pathname length */
#define MAXPATHLEN PATH_MAX

/* I need os-specific macros for sleep/wakeup. */
#define SELPROC struct wait_queue * 
#define CONDITION struct wait_queue *
#define SELWAKEUP(proc) if (proc) wake_up_interruptible(&(SELPROC)(proc));
#define WAKEUP(cond) wake_up_interruptible((cond));
#define SLEEP(cond)  interruptible_sleep_on((cond))



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


#if 0
#undef MUTEX
#define MUTEX struct mymutex
#define MUTEX_INIT(m) (m)->lock_data = 0; (m)->wait = NULL;


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
#endif 0



#ifdef KERNEL
#ifdef assert
#undef assert
#endif assert

#define assert(cond) 							\
    if (!(cond)) {    							\
	printf("Assert at line \"%s\", line %d\n", __FILE__, __LINE__); \
	for (;;) ; \
    }
#endif KERNEL
/* HACK because linux insists on writing dir entries to user space. */

#define CHECK_USER_BUFFER(buf, size) \
  { \
     int tmp; \
     if (bufsize < DIR_PAGE_SIZE) \
	return -EINVAL; \
    tmp = verify_area(VERIFY_WRITE, buf, sizeof(size)); \
    if (tmp) return tmp; \
  }

#endif _LINUX_CODA_FS

