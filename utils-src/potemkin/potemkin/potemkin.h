/************************************* fid table entry */

#ifdef _POSIX_SOURCE
#ifndef MAXNAMLEN
#define MAXNAMLEN  255
#endif  /* MAXNAMLEN */
#endif  /* _POSIX_SOURCE */

#ifndef V_BLKSIZE  
#define V_BLKSIZE  8192
#endif  /* V_BLKSIZE */

typedef struct fid_ent_s {
    ViceFid           fid;
    enum vtype        type;
    ds_list_t        *kids;
    struct fid_ent_s *parent;
    char              name[MAXNAMLEN+1];
} fid_ent_t;

#if defined(__NetBSD__) && defined(__i386__)
#define SYS_STRING  "i386_nbsd1"
#endif

#ifdef __STDC__
#define assert(b)                                           \
do {                                                        \
    if (!(b)) {                                             \
	fprintf(stderr,"assert(%s) -- line %d, file %s\n",  \
                #b, __LINE__, __FILE__);                    \
	zombify();                                          \
    }                                                       \
} while (0)
#else /* __STDC__ */
#define assert(b)                                              \
do {                                                           \
    if (!b) {                                                  \
	fprintf(stderr,"assertion failed line %d, file %s\n",  \
		__LINE__, __FILE__);                           \
	zombify();                                             \
    }                                                          \
} while (0)
#endif

#ifdef LINUX
static void coda_iattr_to_vattr(struct iattr *, struct vattr *);
#define sigcontext sigaction
#define MOUNT_CFS 0
#define d_namlen d_reclen
#define SYS_STRING "linux"
#define ts_sec tv_sec
#define ts_nsec tv_nsec
#endif




