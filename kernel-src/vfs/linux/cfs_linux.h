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
 * however, Venus builds its own VFS interface, modeled on BSD44 
 */
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/sched.h> 
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/wait.h>		
#include <linux/types.h>
#include <linux/fs.h>

/* directory entries; should go somewhere else XXX */
#define u_int32_t   unsigned int
#define u_int16_t   unsigned short
#define u_int8_t    char


/* debugging aids */

#define coda_panic printk


/* debugging masks */
#define D_SUPER     1   /* print results returned by Venus */ 
#define D_INODE     2   /* print entry and exit into procedure */
#define D_FILE      4   /* print malloc, de-alloc information */
#define D_CACHE     8   /* cache debugging */
#define D_MALLOC    16
#define D_CNODE     32
#define D_UPCALL    64  /* up and downcall debugging */
#define D_PSDEV    128  
#define D_PIOCTL   256
#define D_SPECIAL  512

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
    if(coda_print_entry) printk("Process %d entered %s\n",current->pid,__FUNCTION__)

#define EXIT    \
    if(coda_print_entry) printk("Process %d leaving %s\n",current->pid,__FUNCTION__)

extern int coda_print_entry;
extern int coda_debug;
void coda_load_creds(struct coda_cred *cred);
char *coda_f2s(struct ViceFid *, char *);
/* 
 * process and credentials stuff 
 */

#define GLOBAL_PROC current

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

/* Linux doesn't support Sun's VFS: a vnode is just an inode anyway */
/* these macros should only be called for Linux in core inodes */

/* inode to cnode */
#define ITOC(the_inode)  ((struct cnode *)(the_inode)->u.generic_ip)
/* cnode to inode */
#define CTOI(the_cnode)  ((the_cnode)->c_vnode)

#define coda_panic printk
#define CHECK_CNODE(c)                                                \
do {                                                                  \
  struct cnode *cnode = (c);                                          \
  if (!cnode)                                                         \
    coda_panic ("%s(%d): cnode is null\n", __FUNCTION__, __LINE__);        \
  if (cnode->c_magic != CODA_CNODE_MAGIC)                             \
    coda_panic ("%s(%d): cnode magic wrong\n", __FUNCTION__, __LINE__);    \
  if (!cnode->c_vnode)                                                \
    coda_panic ("%s(%d): cnode has null inode\n", __FUNCTION__, __LINE__); \
  if ( (struct cnode *)cnode->c_vnode->u.generic_ip != cnode )           \
    coda_panic("AAooh, %s(%d) cnode doesn't link right!\n", __FUNCTION__,__LINE__);\
} while (0);

#define CN_INIT(CP)        memset((CP), 0, (int)sizeof(struct cnode))

typedef struct inode *C_VNODE_T;
#define LINKS  struct cnode *c_next     /* needed in cnode.h */

#define ITOV(ip) 
#define VTOI(vp) 
#define vm_info_init(a) 
#define VM_INFO_NULL NULL
#define inode_uncache_try(a) ETXTBSY

#define CTOV(the_cp)        CTOI((the_cp))
#define VTOC(vp)        ITOC(vp)
#define CNODE_NEXT(cp)  ((cp)->c_next)

/* ioctl stuff */
/* this needs to be sorted out XXXX */ 

/*
 * Macros to manipulate the queue 
 */


/* circular queues */

struct queue {
    struct queue *forw, *back;
};

#define INIT_QUEUE(head)                     \
do {                                         \
    (head).forw = (struct queue *)&(head);   \
    (head).back = (struct queue *)&(head);   \
} while (0)

#define GETNEXT(head) (head).forw

#define EMPTY(head) ((head).forw == &(head))

#define EOQ(el, head) ((struct queue *)(el) == (struct queue *)&(head))

#define INSQUE(el, head)                             \
do {                                                 \
	(el).forw = ((head).back)->forw;             \
	(el).back = (head).back;                     \
	((head).back)->forw = (struct queue *)&(el); \
	(head).back = (struct queue *)&(el);         \
} while (0)

#define REMQUE(el)                         \
do {                                       \
	((el).forw)->back = (el).back;     \
	(el).back->forw = (el).forw;       \
}  while (0)




/*
 * cfid structure:
 * This overlays the fid structure (see vfs.h)
 */

struct cfid {
    u_short	cfid_len;
    u_short     padding;
    struct ViceFid	cfid_fid;
};

#define MAXSYMLINKS 16
#define VN_TYPE(vp)        (vp)->v_type


#define CODA_ALLOC(ptr, cast, size)                                       \
do {                                                                      \
    if (size < 3000) {                                                    \
        ptr = (cast)kmalloc((unsigned long) size, GFP_KERNEL);            \
                CDEBUG(D_MALLOC, "kmalloced: %x at %x.\n", (int) size, (int) ptr);\
     }  else {                                                             \
        ptr = (cast)vmalloc((unsigned long) size);                        \
	CDEBUG(D_MALLOC, "vmalloced: %x at %x.\n", (int) size, (int) ptr);}\
    if (ptr == 0) {                                                       \
        coda_panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
    memset( ptr, 0, size );                                                   \
} while (0)


#define CODA_FREE(ptr,size) do {if (size < 3000) { kfree_s((ptr), (size)); CDEBUG(D_MALLOC, "kfreed: %x at %x.\n", (int) size, (int) ptr); } else { vfree((ptr)); CDEBUG(D_MALLOC, "vfreed: %x at %x.\n", (int) size, (int) ptr);} } while (0)

#define crfree(cred) CODA_FREE( (cred), sizeof(struct ucred))



/*  */
#define VN_HOLD(ip) iget(coda_super_block, (ip)->i_ino); 
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


/* Venus - Kernel communication macros */



#define DIRBLKSIZ 1024  /*  from BSD44 1.2 dirent.h */


#define Process_pid    current->pid
#define Process_pgid   current->gid

/* Linux in-lines protection stuff, no struct identity */
/* Should this be os specific? */
#define NGROUPS 32
/* #define ucred coda_cred */



/* Maximal pathname length */
#define MAXPATHLEN PATH_MAX

/* I need os-specific macros for sleep/wakeup. */
#define SELWAKEUP(proc) if (proc) wake_up(&(struct wait_queue *)(proc));
#define WAKEUP(cond) wake_up((cond));
#define SLEEP(cond)  interruptible_sleep_on((cond));




#endif _LINUX_CODA_FS

