/*
 * Cnode lookup stuff.
 * NOTE: CFS_CACHESIZE must be a power of 2 for cfshash to work!
 */
#define CFS_CACHESIZE 512

/* 
 * getting from one to the other
 */
#define	VTOC(vp)	((struct cnode *)(vp)->v_data)
#define	CTOV(cp)	((struct vnode *)((cp)->c_vnode))

/*************** allocation. */
#define CFS_ALLOC(ptr, cast, size)                                        \
do {                                                                      \
    ptr = (cast)malloc((unsigned long) size, M_CFS, M_WAITOK);            \
    if (ptr == 0) {                                                       \
	panic("kernel malloc returns 0 at %s:%d\n", __FILE__, __LINE__);  \
    }                                                                     \
} while (0)

#define CFS_FREE(ptr, size)  free((ptr), M_CFS)

/*
 * global cache state control
 */
extern int cfsnc_use;

/*
 * Used to select debugging statements throughout the cfs code.
 */
extern int cfsdebug;
extern int cfsnc_debug;
extern int cfs_printf_delay;
extern int cfs_vnop_print_entry;
extern int cfs_psdev_print_entry;
extern int cfs_vfsop_print_entry;

#define CFSDBGMSK(N)            (1 << N)
#define CFSDEBUG(N, STMT)       { if (cfsdebug & CFSDBGMSK(N)) { STMT } }
#define myprintf(args)          \
do {                            \
    if (cfs_printf_delay)       \
	delay(cfs_printf_delay);\
    printf args ;               \
} while (0)

struct cfs_clstat {
	int	ncalls;			/* client requests */
	int	nbadcalls;		/* upcall failures */
	int	reqs[CFS_NCALLS];	/* count of each request */
};
extern struct cfs_clstat cfs_clstat;


/* WARNING 
 * These macros assume the presence of a process pointer p!
 * And, they're wrong to do so.  Phhht.
 */
#define INIT_IN(in, op, ident) \
	  (in)->opcode = (op); \
	  (in)->pid = p ? p->p_pid : -1; \
          (in)->pgid = p ? p->p_pgid : -1; \
          if (ident != NOCRED) {                              \
	      (in)->cred = *(ident);                          \
          } else {                                            \
	      bzero(&((in)->cred),sizeof(struct CodaCred));   \
	      (in)->cred.cr_uid = -1;                         \
	      (in)->cred.cr_gid = -1;                         \
          }                                                   \

/* Macros to manipulate the queue */
#ifndef INIT_QUEUE
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

#endif INIT_QUEUE

/*
 * Odyssey can have multiple volumes mounted per device (warden). Need
 * to track both the vfsp *and* the root vnode for that volume. Since
 * there is no way of doing that, I felt trading efficiency for
 * understanding was good and hence this structure, which must be
 * malloc'd on every mount.  But hopefully mounts won't be all that
 * frequent (?). -- DCS 11/29/94 
 */

struct ody_mntinfo {
        struct vnode 	   *rootvp;
	struct mount              *vfsp;
	struct ody_mntinfo *next;
};

#define ADD_VFS_TO_MNTINFO(MI, VFS, VP)                                   \
do {                                                                      \
    if ((MI)->mi_vfschain.next) {                                         \
	struct ody_mntinfo *op;                                           \
	                                                                  \
        CFS_ALLOC(op, struct ody_mntinfo *, sizeof (struct ody_mntinfo)); \
	op->vfsp = (VFS);                                                 \
	op->rootvp = (VP);                                                \
	op->next = (MI)->mi_vfschain.next;                                \
	(MI)->mi_vfschain.next = op;                                      \
    } else { /* First entry, add it straight to mnttbl */                 \
	(MI)->mi_vfschain.vfsp = (VFS);                                   \
	(MI)->mi_vfschain.rootvp = (VP);                                  \
    }                                                                     \
} while (0)

	/* get these to cfs_psdev.c */
struct vmsg {
    struct queue vm_chain;
    caddr_t	 vm_data;
    u_short	 vm_flags;
    u_short      vm_inSize;	/* Size is at most 5000 bytes */
    u_short	 vm_outSize;
    u_short	 vm_opcode; 	/* copied from data to save ptr lookup */
    int		 vm_unique;
    caddr_t	 vm_sleep;	/* Not used by Mach. */
};

#define	VM_READ	    1
#define	VM_WRITE    2
#define	VM_INTR	    4

struct vcomm {
	u_long		vc_seq;
	struct selinfo	vc_selproc;
	struct queue	vc_requests;
	struct queue	vc_replys;
};


#define	VC_OPEN(vcp)	    ((vcp)->vc_requests.forw != NULL)
#define MARK_VC_CLOSED(vcp) (vcp)->vc_requests.forw = NULL;
#define MARK_VC_OPEN(vcp)    /* MT */
	/* get these to cfs_psdev.c */
/*
 * CFS structure to hold mount/file system information
 */
struct cfs_mntinfo {
    int			mi_refct;
    struct vcomm	mi_vcomm;
    char		*mi_name;      /* FS-specific name for this device */
    struct ody_mntinfo	mi_vfschain;   /* List of vfs mounted on this device */
};

extern struct cfs_mntinfo cfs_mnttbl[]; /* indexed by minor device number */

/*
 * vfs pointer to mount info
 */
#define vftomi(vfsp)    ((struct cfs_mntinfo *)(vfsp->mnt_data))

/*
 * vnode pointer to mount info
 */
#define vtomi(vp)       ((struct cfs_mntinfo *)(vp->v_mount->mnt_data))

#define	CFS_MOUNTED(vfsp)   (vftomi((vfsp)) != (struct cfs_mntinfo *)0)


/*
 * Used for identifying usage of "Control" object
 */
extern struct vnode *cfs_ctlvp;
#define	IS_CTL_VP(vp)		((vp) == cfs_ctlvp)
#define	IS_CTL_NAME(vp, name, l)(((vp) == vtomi((vp))->mi_vfschain.rootvp)    \
				 && strncmp(name, CFS_CONTROL, l) == 0)

/* 
 * An enum to tell us whether something that will remove a reference
 * to a cnode was a downcall or not
 */
enum dc_status {
    IS_DOWNCALL = 6,
    NOT_DOWNCALL = 7
};


/* Prototypes of functions exported within cfs */
extern int handleDownCal(int opcode, union cfs_downcalls *out);
extern struct cnode *makecfsnode(ViceFid *, struct mount *, short);
extern int cfscall(struct cfs_mntinfo *, int , int *, char *);
extern int  cfs_vmflush __P(());
extern void cfs_flush __P((enum dc_status));
extern void cfs_testflush __P(());
extern void cfs_save __P((struct cnode *));
extern int  cfs_vnodeopstats_init __P(());
extern int  cfs_kill __P((struct mount *, enum dc_status dcstat));
extern void cfs_unsave __P((struct cnode *));
extern int  getNewVnode __P((struct vnode **));
extern void print_vattr __P((struct vattr *));
extern void cfs_free __P((struct cnode *));
