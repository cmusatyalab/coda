#define _SCALAR_T_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef __linux__
#include <mntent.h>
#else /* __linux__ */
#include <sys/file.h>
#include <sys/uio.h>
#endif /* __linux__ */

#include <ds_list.h>
#include <ds_hash.h>

#include "vcrcommon.h"

#include <cfs/coda.h>
/* #include <cfs/cnode.h> */

#include "potemkin.h"

/*
 * WARNING:
 *
 * It is likely that this code will not compile under anything but
 * NetBSD.  In particular, the details of mounting/unmounting are
 * probably different, at least
 */

/************************************************ Argument globals */
char       *KernDevice = "/dev/cfs0";                    /* -kd */
char       *RootDir    = "/tmp/coda";                    /* -rd */
int         FidTabSize = 255;                            /* -ts */
char       *MountPt    = "/coda";                        /* -mp */
int         Interval   = 30;                             /* -i  */
int         verbose    = 0;                              /* -v  */

/************************************************ Other globals */
ds_hash_t        *FidTab;         /* Table of known vnodes, by fid */
unsigned long     Uniq=2;         /* Next uniqifier/vnode to assign */
unsigned long     Volno=1;        /* Which volume to use */
int               KernFD=-1;      /* how to contact the kernel */
ViceFid          *RootFid=NULL;   /* Root fid */
fid_ent_t        *RootFep=NULL;   /* Root fid-entry */

/************************************************ Zombify */

void
zombify()
{
    int              living_dead = 1;
    unsigned int     sleep_interval = 1000000; /* Doesn't matter */

    fprintf(stderr,"Zombifying....\n");
    fflush(stderr);
    fflush(stdout);

    while (living_dead) {
	sleep(sleep_interval);
    }
}	

/************************************************ Fid Hash routines */

long
fid_hash(void *a) {
    fid_ent_t *f = a;
    
    return(f->fid.Unique);
}

long
fid_comp(void *a1, void *a2) {
    fid_ent_t *f1 = a1;
    fid_ent_t *f2 = a2;

    return((long)f1->fid.Unique - (long)f2->fid.Unique);
}

fid_ent_t *
fid_create(char *name, fid_ent_t *parent)
{
    fid_ent_t  *result;
    result = (fid_ent_t *) malloc (sizeof(fid_ent_t));
    result->fid.Volume = Volno;
    result->fid.Vnode = result->fid.Unique = Uniq++;
    result->type = C_VNON;
    result->kids = ds_list_create(fid_comp, TRUE, FALSE);
    result->parent = parent;
    assert(strlen(name) <= MAXNAMLEN);
    strncpy(result->name,name,MAXNAMLEN);
    /* Ensure that there is a null */
    result->name[MAXNAMLEN] = '\0';
    return result;
}

/* 
 * Given a fid, construct it's full name relative to the root. 
 * 
 * Caller is responsible for deallocating return value.
 */

char *
fid_fullname(fid_ent_t *fep)
{
    char             *result;    /* Where we put the answer */
    int               length=0;  /* How long the pathname is so far */
    int               depth=0;   /* How deep the requested fid is */
    fid_ent_t         *fidlist[MAXNAMLEN];
    char              *dest;     /* Where to copy next component. */
    char              *src;      /* Where to copy component from */
    int                i;

    /* 
     * We know that we can't have more than MAXPATHLEN entries, 'cause
     * each entry is minimally "/" 
     */

    result = malloc(sizeof(char)*MAXNAMLEN);
    dest = result;

    do {
	fidlist[depth++] = fep;
	fep = fep->parent;
	assert(depth <= MAXNAMLEN);
    } while (fep != NULL);

    for (i=depth-1; i>=0; --i) {
	length += strlen(fidlist[i]->name)+1;  /* component + '/' or '\0' */
	assert(length < (MAXNAMLEN-1));
	src = fidlist[i]->name;
	while (*src) {
	    *dest++ = *src++;   /* component */
	}
	*dest++ = '/';    /* Trailing '/' */
    }
    *--dest = '\0'; /* blow away trailing '/', replace with '\0' */

    return result;
}

/* 
 * fid_assign_type
 * 
 * takes a fid_ent_t * and a struct stat *, and assigns an appropriate
 * type to the fid_ent_t's type field based on the mode bits of the
 * stat buffer.  Returns nonzero if the stat buffer's mode bit encode
 * any of the following types:
 *
 *    S_IFSOCK, S_IFIFO, S_IFCHR, S_IFBLK
 */
int
fid_assign_type(fid_ent_t *fep, struct stat *sbuf) {

    switch (sbuf->st_mode & S_IFMT) {
    case S_IFDIR:
	fep->type = C_VDIR;
	break;
    case S_IFREG:
	fep->type = C_VREG;
	break;
    case S_IFLNK:
	fep->type = C_VLNK;
	break;
    case S_IFSOCK:               /* Can't be any of these */
    case S_IFIFO:
    case S_IFCHR:
    case S_IFBLK:
	fep->type = C_VBAD;
	return -1;
	break;
    default:
	assert(0);
	break;
    }
    return 0;
}

/* Dumping the FidTab */

void
fid_print(FILE *ostr, fid_ent_t *fep)
{
    char             *typestr;
    ds_list_iter_t   *i;
    fid_ent_t        *childp;
    char             *fullname;

    switch (fep->type) {
    case C_VDIR:
	typestr = "C_VDIR";
	break;
    case C_VREG:
	typestr = "C_VREG";
	break;
    case C_VLNK:
	typestr = "C_VLNK";
	break;
    default:
	typestr = "????";
	break;
    }

    fprintf(ostr,"(%x.%x.%x)\t%s\t%s\n",
	    fep->fid.Volume, fep->fid.Vnode, fep->fid.Unique,
	    fep->name, typestr);
    fullname = fid_fullname(fep);
    fprintf(ostr,"\t%s\n",fullname);
    free(fullname);
    if (fep->parent) {
	fprintf(ostr,"\tPARENT: (%x.%x.%x)\n",
		fep->parent->fid.Volume,
		fep->parent->fid.Vnode,
		fep->parent->fid.Unique);
    }
    fprintf(ostr,"\t***CHILDREN***\n");
    i = ds_list_iter_create(fep->kids);
    while ((childp = ds_list_iter_next(i)) != NULL) {
	fprintf(ostr,"\t\t(%x.%x.%x)\n",
		childp->fid.Volume, childp->fid.Vnode, childp->fid.Unique);
    }
    ds_list_iter_destroy(i);
    fprintf(ostr,"\n");
}

void
dump_fids(int sig, int code, struct sigcontext *scp)
{
    ds_hash_iter_t  *i;
    fid_ent_t       *fep;
    
    i = ds_hash_iter_create(FidTab);
    while ((fep = ds_hash_iter_next(i)) != NULL) {
	fid_print(stdout,fep);
    }
    fflush(stdout);
}

void
fid_init() {
    FidTab = ds_hash_create(fid_comp, fid_hash, FidTabSize, TRUE, FALSE);
}

/************************************************ Argument parsing, etc. */

void
usage() {
    fprintf(stderr,"Usage: potemkin [-kd <kern device>] [-rd <root>]\n");
    fprintf(stderr,"                [-ts <tab size>] [-mp <mount point>]\n");
    exit(-1);
}

void
ParseArgs(int argc, char *argv[]) {
    int i;

    for (i=1; i < argc; i++) {
	if (!strcmp(argv[i],"-kd")) {
	    if (++i==argc) usage();
	    KernDevice = argv[i];
	} else if (!strcmp(argv[i],"-rd")) {
	    if (++i==argc) usage();
	    RootDir = argv[i];
	} else if (!strcmp(argv[i],"-ts")) {
	    if (++i==argc) usage();
	    FidTabSize = atoi(argv[i]);
	} else if (!strcmp(argv[i],"-mp")) {
	    if (++i==argc) usage();
	    MountPt = argv[i];
	} else if (!strcmp(argv[i],"-i")) {
	    if (++i==argc) usage();
	    Interval = atoi(argv[i]);
	} else if (!strcmp(argv[i],"-v")) {
	    verbose = 1;
	} else {
	    usage();
	}
    }
}

/*************************************************** Initialization */
/*
 * Setup: -- we need to do the following
 * 
 *          1) change to the root directory
 *          2) set up the fid<->file table
 *          3) Test kernel
 *          4) Mount on MountPoint
 */

void
Setup() {
    union outputArgs msg;
    struct sigaction  sa;

    /* Step 1: change to root directory */
    assert(!(chdir(RootDir)));

    /* Step 2: set up the fid<->file table */
    fid_init();

    /* Step 3: Test the kernel */
    /*
     * Open the kernel.
     * Construct a purge message and see if we can send it in... 
     */
    KernFD = open(KernDevice, O_RDWR, 0);
    assert(KernFD >= 0);

    msg.oh.opcode = CFS_FLUSH;
    msg.oh.unique = 0;
    assert (write(KernFD, (char*)&msg, sizeof(struct cfs_out_hdr))
	    == sizeof(struct cfs_out_hdr));

#ifdef __linux__
    if ( fork() == 0 ) {
      int error;
      error = mount("coda", MountPt, "coda",  MS_MGC_VAL , &KernDevice);
      if ( error ) {
	pid_t parent;
	perror("Killing parent, mount error:");
	parent = getppid();
	kill(parent, SIGKILL);
	exit(1);
      } else {
	FILE *fd;
	struct mntent ent;
	fd = setmntent("/etc/mtab", "a");
	if ( fd > 0 ) { 
	  ent.mnt_fsname="Coda";
	  ent.mnt_dir=MountPt;
	  ent.mnt_type= "coda";
	  ent.mnt_opts = "rw";
	  ent.mnt_freq = 0;
	  ent.mnt_passno = 0;
	  error = addmntent(fd, & ent);
	  error = endmntent(fd);
	  exit(0);
	}
      }
    }
#else
    assert (!mount(MOUNT_CFS, MountPt, 0, KernDevice));
#endif

    /* Set up a signal handler to dump the contents of the FidTab */
    sa.sa_handler = dump_fids;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);  /* or -1, who knows? */
    assert (!sigaction(SIGUSR1, &sa, NULL));

    /* Set umask to zero */
    umask(0);
}

/*************************************************** utility */

/* 
 * child_exists:
 *     given a path (assumed to be a directory) and a filename, returns
 *     non-zero if that filename exists in the directory.
 */

int
child_exists(char *path, char *name) {
    DIR               *dirp;
    struct dirent     *dep;
    int                length;

    dirp = opendir(path);
    assert(dirp);
    length = strlen(name);
    do {
	dep = readdir(dirp);
	if (dep 
#ifndef __linux__
	    && (dep->d_namlen == length) 
#endif
	    && (!strcmp(dep->d_name, name)))
	{
	    break;
	}
    } while (dep);
    closedir(dirp);

    /* If we matched... */
    if (dep) {
	return 1;
    }
    return 0;
}

/*
 * fill_coda_vattr
 *
 * Given a stat structure and a fid for a new vnode, fill in it's
 * coda_vattr structure.  WARNING: fep must be completely filled.
 */
void
fill_vattr(struct stat *sbuf, fid_ent_t *fep, struct coda_vattr *vbuf) 
{
    vbuf->va_type = fep->type;
    vbuf->va_mode = sbuf->st_mode;
    vbuf->va_nlink = sbuf->st_nlink;
    vbuf->va_uid = sbuf->st_uid;
    vbuf->va_gid = sbuf->st_gid;
    /* vbuf->va_fileid = fep->fid.Vnode; */
    /* va_fileid has to be the vnode number of the cache file.  Sorry. */
    /*    vbuf->va_fileid = sbuf->st_ino; */
    vbuf->va_fileid = coda_f2i(&(fep->fid));
    vbuf->va_size = sbuf->st_size;
    vbuf->va_blocksize = V_BLKSIZE;
#ifdef __linux__
    vbuf->va_atime.tv_sec = sbuf->st_atime;
    vbuf->va_mtime.tv_sec = sbuf->st_mtime;
    vbuf->va_ctime.tv_sec = sbuf->st_ctime;
#else
    vbuf->va_atime = sbuf->st_atimespec;
    vbuf->va_mtime = sbuf->st_mtimespec;
    vbuf->va_ctime = sbuf->st_ctimespec;
    vbuf->va_flags = sbuf->st_flags;
#endif
    vbuf->va_gen = fep->fid.Unique;
    vbuf->va_rdev = 0; /* Can't have special devices in /coda */
    vbuf->va_bytes = sbuf->st_size; /* Should this depend on cache/uncache? */
    vbuf->va_filerev = fep->fid.Vnode; 
}



/*************************************************** cfs operations */

/* Phase 1 - opening/closing, naming */

/* 
 * Root:
 *        in:   nothing
 *        out:  Fid of root vnode
 */

#define VC_OUTSIZE(name) sizeof(struct name)
#define VC_INSIZE(name)  sizeof(struct name)
#define VC_OUT_NO_DATA   sizeof(struct cfs_out_hdr)
#define VC_IN_NO_DATA    sizeof(struct cfs_in_hdr)

void
DoRoot(union inputArgs *in, union outputArgs *out, int *reply)
{
    fid_ent_t       *root;
    if (!RootFid) {
	root = fid_create(".", NULL);
	assert(ds_hash_insert(FidTab, root));
	root->type = C_VDIR;
	RootFep = root;
	RootFid = &(root->fid);
    }
    out->cfs_root.VFid.Volume = RootFid->Volume;
    out->cfs_root.VFid.Vnode = RootFid->Vnode;
    out->cfs_root.VFid.Unique = RootFid->Unique;
    out->oh.result = 0;

    if (verbose) {
	printf("Returning root: fid (%x.%x.%x)\n",RootFid->Volume,
	       RootFid->Vnode, RootFid->Unique);
    }
    *reply = VC_OUTSIZE(cfs_root_out);
    return;
}

void
DoOpen(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid              *fp;
    int                  *flags;
    fid_ent_t            *fep;
    fid_ent_t             dummy;
    char                 *path=NULL;
    struct stat           sbuf;

    fp = &(in->cfs_open.VFid);
    flags = &(in->cfs_open.flags);

    dummy.fid = *fp;
    assert((fep = ds_hash_member(FidTab, &dummy)) != NULL);
    
    path = fid_fullname(fep);
    if (verbose) {
	printf("Geting dev,inode for fid (%x.%x.%x): %s",fp->Volume,
	       fp->Vnode, fp->Unique, path);
	fflush(stdout);
    }
    if (lstat(path,&sbuf)) {
	out->oh.result = errno;
	goto exit;
    }
    out->cfs_open.dev = sbuf.st_dev;
    out->cfs_open.inode = sbuf.st_ino; 
    out->oh.result = 0;
    if (verbose) {
	printf("....found\n");
	fflush(stdout);
    }
 exit:
    if (path) free(path);
    *reply = VC_OUTSIZE(cfs_open_out);
    return;
}

void
DoClose(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid  *fp;

    fp = &(in->cfs_close.VFid);

    /* Close always succeeds */
    if (verbose) {
	printf("Trival close for fid (%x.%x.%x)\n", fp->Volume,
	       fp->Vnode, fp->Unique);
	fflush(stdout);
    }
    out->oh.result = 0;
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoAccess(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid  *fp;

    fp = &(in->cfs_close.VFid);

    /* Access always succeeds, for now */
    if (verbose) {
	printf("Trival access for fid (%x.%x.%x)\n", fp->Volume,
	       fp->Vnode, fp->Unique);
	fflush(stdout);
    }
    out->oh.result = 0;
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoLookup(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid           *fp;
    char              *name;
    fid_ent_t         *fep;
    fid_ent_t         *childp=NULL;
    bool               created=FALSE;
    fid_ent_t          dummy;
    ds_list_iter_t    *iter;
    char              *path=NULL;
    struct stat        sbuf;
    
    fp = &(in->cfs_lookup.VFid);
    assert((int)in->cfs_lookup.name == VC_INSIZE(cfs_lookup_in));
    name = (char*)in + (int)in->cfs_lookup.name;

    if (verbose) {
	printf("Doing lookup of (%s) in fid (%x.%x.%x)\n",
	       name, fp->Volume, fp->Vnode, fp->Unique);
	fflush(stdout);
    }
    
    /* We'd better be looking up in a directory */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member (FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Step 0: Special case '.' and '..' */
    if (!strcmp(name, ".")) {
	childp = fep;
	goto found;
    }
    if (!strcmp(name, "..")) {
	if (fep == RootFep) {
	    fprintf(stderr,"AACK!  DoLookup of '..' through root!\n");
	    out->oh.result = EBADF;
	    goto exit;
	}
	childp = fep->parent;
	goto found;
    }
    
    /* Step 0.5: Special case '@sys' */
    if (!strcmp(name, "@sys")) {
	name = SYS_STRING;
    }

    /* Begin normal pathname lookup. */

    /* Step 1: have we resolved it before? */
    iter = ds_list_iter_create(fep->kids);
    while ((childp = ds_list_iter_next(iter)) != NULL) {
	if (!strcmp(childp->name, name))
	    break;
    }
    ds_list_iter_destroy(iter);

    if (!childp) {
	/* Didn't find it */
	/* Step 2: look in the actual directory for the child */
	path = fid_fullname(fep);
	if (!child_exists(path,name)) {
	    /* No such child */
	    out->oh.result = ENOENT;
	    goto exit;
	}	    
	/* Create a fid entry for this child */
	childp = fid_create(name, fep);
	created = TRUE;

	if (path) free(path);
	path = fid_fullname(childp);
	if (lstat(path,&sbuf)) {      /* Can't read? */
	    out->oh.result = ENOENT;
	    goto exit;
	}

	if (fid_assign_type(childp, &sbuf)) {
	    out->oh.result = ENOENT;
	    goto exit;
	}

	/* Remember that we have this vnode */
	assert(ds_hash_insert(FidTab, childp));
	assert(ds_list_insert(fep->kids, childp));
    }

 found:
    /* Okay. We now have a valid vnode in childp, we'll succeed */
    out->cfs_lookup.VFid = childp->fid;
    out->cfs_lookup.vtype = childp->type;
    out->oh.result = 0;
    if (verbose) {
	printf("....found\n");
	fflush(stdout);
    }
 exit:
    if (out->oh.result && created && childp)    
    {
	/* Error -- We don't want the child */
	free(childp);
    }
    if (path) free(path);
    *reply = VC_OUTSIZE(cfs_lookup_out);
    return;
}

/* Phase 2a: Reading/Writing */

void
DoGetattr(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid          *fp;
    fid_ent_t        *fep;
    fid_ent_t         dummy;
    struct stat       st;
    struct coda_vattr     *vbuf;
    char             *path = NULL;
    
    fp = &(in->cfs_getattr.VFid);
    vbuf = &(out->cfs_getattr.attr);

    if (verbose) {
	printf("Doing getattr for fid (%x.%x.%x)\n",
	       fp->Volume, fp->Vnode, fp->Unique);
    }
    
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member (FidTab, &dummy)) != NULL);

    path = fid_fullname(fep);
    if (lstat(path,&st)) {
	out->oh.result = errno;
    } else {
	if (fid_assign_type(fep, &st)) {
	    out->oh.result = ENOENT;
	    goto exit;
	}
	fill_vattr(&st, fep, vbuf);
    }
    
    out->oh.result = 0;

 exit:
    if (path) free(path);
    *reply = VC_OUTSIZE(cfs_getattr_out);
    return;
}

void
DoReaddir(union inputArgs *in, union outputArgs *out, int *reply)
{
    /* Do nothing: it's handled by FFS/LFS */
    out->oh.result = EOPNOTSUPP;
    return;
}

void
DoRdwr(union inputArgs *in, union outputArgs *out, int *reply)
{
    /* Do nothing: it's handled by FFS/LFS */
    out->oh.result = EOPNOTSUPP;
    return;
}

/* Phase 2b - namespace changes */

void
DoCreate(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid         *fp;
    fid_ent_t       *fep;
    ViceFid         *newFp;
    fid_ent_t       *newFep=NULL;
    fid_ent_t        dummy;
    struct coda_vattr    *attr;
    struct coda_vattr    *newAttr;
    struct coda_cred    *cred;
    int              exclp;
    int              mode;
    int              readp;
    int              writep;
    int              truncp;
    int              oflags=0;
    int              omode;  /* Shouldn't this be just mode? */
    bool             created=FALSE;
    char            *name=NULL;
    char            *path=NULL;
    ds_list_iter_t  *iter;
    struct stat      sbuf;
    int              fd;
    uid_t            suid;
    gid_t            sgid;

    fp = &(in->cfs_create.VFid);
    attr = &(in->cfs_create.attr);
    cred = &(in->ih.cred);
    exclp = in->cfs_create.excl;
    mode = in->cfs_create.mode;
    readp = mode & C_M_READ;
    writep = mode & C_M_WRITE;
    truncp = (attr->va_size == 0);
    assert((int)in->cfs_create.name == VC_INSIZE(cfs_create_in));
    name = (char*)in + (int)in->cfs_create.name;
    newFp = &(out->cfs_create.VFid);
    newAttr = &(out->cfs_create.attr);

    if (verbose) {
	printf("Doing create of (%s) in fid (%x.%x.%x) mode 0%o %s\n",
	       name, fp->Volume, fp->Vnode, fp->Unique, mode, 
	       (exclp ? "excl" : ""));
    }
    
    /* Where are we creating this? */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	printf("Ack!  Trying to create in a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }
    
    /* Step 0: Is the name-to-be-created okay? */

    /* Don't allow any creation of '.', '..', ''; they already must exist. */
    if ((!strcmp(name, ".")) 
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, "")))
    {
	printf("CREATE: create of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    /* Step 1: does this child already exist? */
    path = fid_fullname(fep);
    /*I'm not currently checking to see that
     * previously-existing-files continue to have the same vtype.
     * (I'll just reset it if it's different, which would be a
     * rather serious disaster.)  
     */
    if (child_exists(path, name)) {
	if (exclp) {
	    out->oh.result = EEXIST;
	    goto exit;
	}
	/* Do we know about the child yet? */
	iter = ds_list_iter_create(fep->kids);
	while ((newFep = ds_list_iter_next(iter)) != NULL) {
	    if (!strcmp(newFep->name, name)) {
		break;
	    }
	}
	ds_list_iter_destroy(iter);

    }	

    if (!newFep) {
	/* 
	 * XXX This is either a completely new file, or a file that we
	 * have not yet created a fid for.  We should create a new
	 * fid_ent_t for it.
	 */
	newFep = fid_create(name, fep);
	created = TRUE;
    }

    /* 
     * Now, we have to set the path to it so that the open-for-create
     * and later stat will happen correctly.  (We'll need to do the
     * stat to correctly set the type of the fep.
     */
    if (path) free (path);
    path = fid_fullname(newFep);

	
    /* 
     * Okay, now do the create.
     * I'll be lazy and do so with an actual open.  (Which we have
     * to do eventually anyway if we are truncating.  Should I worry
     * about this here?  Not sure...)  
     */

    /* Set the open flags */
    /* XXX - this code assumes O_RDONLY is 0 */
    if (writep) {
	if (readp) {
	    oflags |= O_RDWR;
	} else {
	    oflags |= O_WRONLY;
	}
    }
    oflags |= O_CREAT;
    if (truncp) {
	oflags |= O_TRUNC;
    }
    if (exclp) {
	oflags |= O_EXCL;
    }

    /* Set the mode bits */
    omode = mode & 0777;   /* XXX - but that's what the man page says */

    /* Set the creator for this file */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    /* Do the open */
    if ((fd = open(path, oflags, omode)) < 0) {
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    } else {
	close(fd);
    }

    /* Reset our effective uid/gid */
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* Stat the thing so that we can set it's type correctly. */
    /* Complain miserably if it isn't a plain file! */
    if (lstat(path,&sbuf)) {
	out->oh.result = errno;
	goto exit;
    }
    if (fid_assign_type(newFep, &sbuf)) {
	out->oh.result = ENOENT;
	goto exit;
    }
    if (newFep->type != C_VREG) {
	printf("AACK!  Create is 'creating' a non-file file of type %d!\n",
	       newFep->type);
    }
    /* We are now doomed to succeed :-) */
    /* Record this fid, and finish off */
    out->oh.result = 0;
    assert(ds_hash_insert(FidTab, newFep));
    assert(ds_list_insert(fep->kids, newFep));

    /* Set the return values for the create call */
    *newFp = newFep->fid;
    fill_vattr(&sbuf, newFep, newAttr);

 exit:
    if (out->oh.result && created && newFep) {
	/* We don't keep the newFep if there was an error */
	free(newFep);
    }
    if (path) free(path);
    *reply = VC_OUTSIZE(cfs_create_out);
    return;
}

void
DoRemove(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid         *fp;
    fid_ent_t       *fep;
    fid_ent_t       *victimFep=NULL;
    bool             created=FALSE;
    struct coda_cred    *cred;
    fid_ent_t        dummy;
    char            *name=NULL;
    char            *path=NULL;
    ds_list_iter_t  *iter;
    struct stat      sbuf;
    uid_t            suid;
    gid_t            sgid;

    fp = &(in->cfs_remove.VFid);
    cred = &(in->ih.cred);
    assert((int)in->cfs_remove.name == VC_INSIZE(cfs_remove_in));
    name = (char*)in + (int)in->cfs_remove.name;
    
    if (verbose) {
	printf("Doing remove of (%s) in fid (%x.%x.%x)\n",
	       name, fp->Volume, fp->Vnode, fp->Unique);
    }
    
    /* Get the directory from which we are removing */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	printf("REMOVE: parent not directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }
    /* Step 0: Check to ensure that removes don't happen to '.', '..', or '' */
    if ((!strcmp(name, "."))
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, ""))) 
    {
	printf("REMOVE: remove of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    path = fid_fullname(fep);
    /* Step 1: find the child */
    if (!child_exists(path,name)) {
	printf("REMOVE: child didn't exist!\n");
	out->oh.result = ENOENT;
	goto exit;
    }
    /* Do we know about the child yet? */
    iter = ds_list_iter_create(fep->kids);
    while ((victimFep = ds_list_iter_next(iter)) != NULL) {
	if (!strcmp(victimFep->name, name)) {
	    break;
	}
    }
    ds_list_iter_destroy(iter);
    
    if (!victimFep) {
	/* 
	 * We're going to create a victimFep as a temporary; this is
	 * only to make the code simpler.  I know that it isn't
	 * that efficient, but...
	 */
	victimFep = fid_create(name, fep);
	created = TRUE;
    }

    /* 
     * Get the path for the unlink call.  Ensure it is a regular file.
     * We are *not* allowed to unlink anything else here.
     */
    path = fid_fullname(victimFep);
    if (lstat(path,&sbuf)) {
	out->oh.result = errno;
	goto exit;
    }
    switch(sbuf.st_mode & S_IFMT) {
    case S_IFREG:
    case S_IFLNK:
	/* This might not be okay for symlinks. */
	break;
    case S_IFDIR:
	printf("REMOVE: trying to remove a directory!\n");
	out->oh.result = EINVAL;
	goto exit;
	break;
    case S_IFSOCK:               /* Can't be any of these */
    case S_IFIFO:
    case S_IFCHR:
    case S_IFBLK:
	printf("REMOVE: trying to remove an esoteric!\n");
	out->oh.result = EINVAL;
	goto exit;
	break;
    default:
	assert(0);
	break;
    }
    
    
    /* Do the unlink.  Need to try it as the user calling us */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    if (unlink(path)) {
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    }
    
    /* reset our effictive credentials */
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* We are now doomed to succeed */
    /* If the victimFep was already known, remove & destroy it */
    if (created == FALSE) {
	ds_list_remove(fep->kids, victimFep);
	ds_hash_remove(FidTab, victimFep);
    }
    out->oh.result = 0;

 exit:
    if (!out->oh.result) {
	/* If successful, reclaim this fid entry */
	if (victimFep) free(victimFep);
    } else {
	/* reclaim it if we *weren't* successful, but created the fid */
	if (created == TRUE && victimFep) free(victimFep);
    }
    if (path) free(path);
    /* There is no extra info to return */
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoSetattr(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid          *fp;
    struct coda_cred     *cred;
    struct coda_vattr     *vap;
    fid_ent_t        *fep;
    fid_ent_t         dummy;
    char             *path=NULL;
    uid_t             suid;
    gid_t             sgid;
    struct timeval    times[2];

    fp = &(in->cfs_setattr.VFid);
    cred = &(in->ih.cred);
    vap = &(in->cfs_setattr.attr);

    if (verbose) {
	printf("Doing setattr for fid (%x.%x.%x)\n",
	       fp->Volume, fp->Vnode, fp->Unique);
    }
    
    /* 
     * Check to see that we aren't changing anything we aren't
     * supposed to.  This list came from Venus (vproc_vfscalls.c)
     */

    /* 
     * I ignore the following fields, since I'm not sure what to do
     * about them: va_filerev va_vaflags 
     */

    /*
     * One exception: we silently allow va_flags to be "set", even
     * though we can do nothing about them, since they don't make
     * sense to us.  Hopefully, no one will ever notice, though I'm
     * suspicious.  We only return silently if this is the *only*
     * thing of interest which we are setattr'ing.  
     */
    if ((vap->va_flags != (unsigned long)-1) &&
	(vap->va_mode == (u_short)-1) &&
	(vap->va_uid == (uid_t)-1) &&
	(vap->va_gid == (gid_t)-1) &&
	(vap->va_size == (off_t)-1) &&
	(vap->va_atime.tv_sec == (long)-1) &&
	(vap->va_mtime.tv_sec == (long)-1) &&
	(vap->va_ctime.tv_sec == (long)-1))
    {
	out->oh.result = 0;
	goto earlyexit;
    }

    if ((vap->va_type != C_VNON) ||
	(vap->va_fileid != (long)-1) ||
	(vap->va_gen != (long)-1) ||
	(vap->va_bytes != (long)-1) ||
	(vap->va_nlink != (short)-1) ||
	(vap->va_blocksize != (long)-1) ||
	(vap->va_rdev != (dev_t)-1))
    {
	out->oh.result = EINVAL;
	goto earlyexit;
    }

    /* 
     * Check to make sure at least something we care about is set.
     */
    if ((vap->va_mode == (u_short)-1) &&
	(vap->va_uid == (uid_t)-1) &&
	(vap->va_gid == (gid_t)-1) &&
	(vap->va_size == (off_t)-1) &&
	(vap->va_atime.tv_sec == (long)-1) &&
	(vap->va_mtime.tv_sec == (long)-1) &&
	(vap->va_ctime.tv_sec == (long)-1))
    {
	out->oh.result = EINVAL;
	printf("SETATTR: nothing set!\n");
	goto earlyexit;
    }

    /* Get the object to setattr */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    path = fid_fullname(fep);

    /* 
     * Not sure how I want to handle authentication.  For now, let's
     * just use the standard UNIX rules about root being the only one
     * to do certain things, and owner being the only other one to do
     * others.  WARNING: if somethings fail, but others succeed, then
     * perhaps only part of the setattr will happen...this is bad, but 
     * we can live with it for the purposes of potemkin.
     */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    /* Are we truncating the file? */
    if ((u_long)vap->va_size != (u_long)-1) {
	/* Is it either a directory or a symlink? */
	if (fep->type != C_VREG) {
	    printf("SETATTR: Setting length of something not C_VREG\n");
	    out->oh.result = (fep->type == C_VDIR) ? EISDIR : EINVAL;
	    goto exit;
	}
	/* Try to do the truncate */
	if (truncate(path, vap->va_size)) {
	    out->oh.result = errno;
	    printf("SETATTR: Truncate of (%s) failed\n",path);
	    goto exit;
	}
    }

    /* Are we setting owner/group? */
    if ((vap->va_uid != (uid_t)-1) || (vap->va_gid != (gid_t)-1)) {
	/* As long as it's not a symlink. */
	if (fep->type == C_VLNK) {
	    printf("SETATTR: chown(2) of a symlink\n");
	    out->oh.result = EINVAL;
	    goto exit;
	}
	if (chown(path,vap->va_uid,vap->va_gid)) {
	    printf("SETATTR: chown failed\n");
	    out->oh.result = errno;
	    goto exit;
	}
    }
    
    /* Are we setting the mode bits? */
    if (vap->va_mode != (u_short)-1) {
	if (chmod(path,vap->va_mode)) {
	    printf("SETATTR: chmod failed\n");
	    out->oh.result = errno;
	    goto exit;
	}
    }
    
    /* Are we setting the mtime/atime? */
    if ((vap->va_atime.tv_sec != -1) || (vap->va_mtime.tv_sec != -1)) {
	times[0].tv_sec = vap->va_atime.tv_sec;
	times[0].tv_usec = vap->va_atime.tv_nsec/1000;
	times[1].tv_sec = vap->va_mtime.tv_sec;
	times[1].tv_usec = vap->va_mtime.tv_nsec/1000;
	if (utimes(path, times)) {
	    printf("SETATTR: chmod failed\n");
	    out->oh.result = errno;
	    goto exit;
	}
    }

    /* We are now going to succeed */
    out->oh.result = 0;
    
 exit:
    assert(!seteuid(suid));
    assert(!setgid(sgid));
 earlyexit:
    if (path) free(path);
    *reply = VC_OUT_NO_DATA;

    return;
}

void
DoRename(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid          *sdfp;
    fid_ent_t        *sdfep;
    ViceFid          *tdfp;
    fid_ent_t        *tdfep;
    fid_ent_t         dummy;
    char             *sname;
    char             *spath=NULL;
    char             *tname;
    char             *tpath=NULL;
    fid_ent_t        *sfep=NULL;
    bool              screated=FALSE;
    fid_ent_t        *tfep=NULL;
    bool              tcreated=FALSE;
    struct coda_cred     *cred;
    ds_list_iter_t   *iter;
    struct stat       sbuf;
    uid_t             suid;
    gid_t             sgid;
    
    sdfp = &(in->cfs_rename.sourceFid);
    tdfp = &(in->cfs_rename.destFid);
    cred = &(in->ih.cred);
    sname = (char*)in + (int)in->cfs_rename.srcname;
    tname = (char*)in + (int)in->cfs_rename.destname;

    if (verbose) {
	printf("Rename: moving %s from (%x.%x.%x) to %s in (%x.%x.%x)\n",
		sname, sdfp->Volume, sdfp->Vnode, sdfp->Unique,
		tname, tdfp->Volume, tdfp->Vnode, tdfp->Unique);
    }

    /* Grab source directory, make sure it's a directory. */
    dummy.fid = *sdfp;
    assert((sdfep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (sdfep->type != C_VDIR) {
	printf("Ack!  Trying to rename something from a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Grab target directory, make sure it's a directory. */
    dummy.fid = *tdfp;
    assert((tdfep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (tdfep->type != C_VDIR) {
	printf("Ack!  Trying to rename something from a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Step 0: Is the name-to-be-created okay? */

    /* Don't allow any creation of '.', '..', ''; they already must exist. */
    if ((!strcmp(tname, ".")) 
	|| (!strcmp(tname, ".."))
	|| (!strcmp(tname, "")))
    {
	printf("CREATE: create of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(tname) == 27) 
	&& (tname[0] == '@') 
	&& (tname[9] == '.') 
	&& (tname[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    /* Step 1: does the target exist, and do we know about it */
    /* If so, we'll remove it at the end on success */
    iter = ds_list_iter_create(tdfep->kids);
    while ((tfep = ds_list_iter_next(iter)) != NULL) {
	if (!strcmp(tfep->name, tname)) {
	    break;
	}
    }
    ds_list_iter_destroy(iter);

    if (!tfep) {
	tfep = fid_create(tname, tdfep);
	tcreated = TRUE;
    }
    tpath = fid_fullname(tfep);

    /* Step 2: do we have record of the source fid yet? */
    iter = ds_list_iter_create(sdfep->kids);
    while ((sfep = ds_list_iter_next(iter)) != NULL) {
	if (!strcmp(sfep->name, sname)) {
	    break;
	}
    }
    ds_list_iter_destroy(iter);

    if (!sfep) {
	sfep = fid_create(sname, sdfep);
	screated = TRUE;
    }
    spath = fid_fullname(sfep);

    /* 
     * Step 3a: stat the source file for it's type.  Must happen
     * *before* we try to do the rename, 'cause if the stat fails,
     * we're hosed 
     */
    if (stat(spath,&sbuf)) {
	printf("RENAME: stat of source failed (%s)\n",strerror(errno));
	out->oh.result = errno;
	goto exit;
    }

    /* Step 3: try to do the rename. */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    if (rename(spath,tpath)) {
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	out->oh.result = errno;
	printf("RENAME: rename failed %s\n",strerror(errno));
	goto exit;
    }
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* We will now succeed */
    out->oh.result = 0;

    /* If we didn't create the tfep, we must remove it */
    if (tcreated == FALSE) {
	ds_list_remove(tdfep->kids, tfep);
	ds_hash_remove(FidTab, tfep);
    }

    /* If we didn't create the the sfep, we must remove it from sdfep's list */
    if (screated == FALSE) {
	ds_list_remove(sdfep->kids, sfep);
    } else {
	/* We need to fill in a type for the new fid. */
	fid_assign_type(sfep,&sbuf);
	/* And enter it into the fid tab */
	ds_hash_insert(FidTab,sfep);
    }
    /* Change sfep's name, and insert the sfep into the tdfep directory */
    strcpy(sfep->name,tname);
    sfep->parent = tdfep;
    ds_list_insert(tdfep->kids, sfep);

 exit:
    /* If we succeeded, we must toast the tfep */
    if (!out->oh.result) {
	free(tfep);
    } else {
	/* 
	 * If we failed, then we must remove tfep and sfep if they
	 * were created as side effects of this operation
	 */
	if (tcreated == TRUE && tfep) free(tfep);
	if (screated == TRUE && sfep) free(sfep);
    }
    if (spath) free(spath);
    if (tpath) free(tpath);
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoMkdir(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid          *fp;
    fid_ent_t        *fep;
    ViceFid          *newFp;
    fid_ent_t        *newFep=NULL;
    fid_ent_t         dummy;
    struct coda_vattr     *attr;
    struct coda_vattr     *newAttr;
    struct coda_cred     *cred;
    int               mode;
    char             *name=NULL;
    char             *path=NULL;
    struct stat       sbuf;
    uid_t             suid;
    gid_t             sgid;
    
    fp = &(in->cfs_mkdir.VFid);
    attr = &(in->cfs_mkdir.attr);
    cred = &(in->ih.cred);
    assert((int)in->cfs_mkdir.name == VC_INSIZE(cfs_mkdir_in));
    name = (char*)in + (int)in->cfs_mkdir.name;
    newFp = &(out->cfs_mkdir.VFid);
    newAttr = &(out->cfs_mkdir.attr);
    mode = attr->va_mode & 07777; /* XXX, but probably not */

    if (verbose) {
	printf("Doing mkdir of (%s) in fid (%x.%x.%x)\n",
	       name, fp->Volume, fp->Vnode, fp->Unique);
    }

    /* Where are we creating this? */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	printf("Ack!  Trying to mkdir in a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Step 0: Is the name-to-be-created okay? */

    /* Don't allow any creation of '.', '..', ''; they already must exist. */
    if ((!strcmp(name, ".")) 
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, "")))
    {
	printf("MKDIR: create of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	printf("MKDIR: create of fakified-like name\n");
	out->oh.result = EINVAL;
	goto exit;
    }

    /* Step 1: does this child already exist?  For mkdir, it cannot */
    path = fid_fullname(fep);
    if (child_exists(path, name)) {
	printf("MKDIR: child exists\n");
	out->oh.result = EEXIST;
	goto exit;
    }

    /* Go ahead and create it, but don't enter yet. */
    newFep = fid_create(name, fep);
    
    /* New path for the directory-to-be-created */
    if (path) free (path);
    path = fid_fullname(newFep);

    /* Set the creator for this file */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    /* Do the mkdir */
    if (mkdir(path,mode)) {
	printf("MKDIR: mkdir failed (%s)\n",strerror(errno));
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    }

    /* Reset our effective uid/gid */
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* Stat it so we can set type, return coda_vattr correctly */
    if (lstat(path,&sbuf)) {
	printf("MKDIR: couldn't lstat %s: (%s)\n",
	       path,strerror(errno));
	out->oh.result = errno;
	goto exit;
    }
    if (fid_assign_type(newFep, &sbuf)) {
	out->oh.result = ENOENT;
	goto exit;
    }

    /* We are now doomed to succeed :-) */
    /* Record this fid, and finish off */
    out->oh.result = 0;
    assert(ds_hash_insert(FidTab, newFep));
    assert(ds_list_insert(fep->kids, newFep));

    /* Set the return values for the create call */
    *newFp = newFep->fid;
    fill_vattr(&sbuf, newFep, newAttr);

 exit:
    if (out->oh.result && newFep) free(newFep);
    if (path) free (path);
    *reply = VC_OUTSIZE(cfs_mkdir_out);
    return;
}

void
DoRmdir(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid           *fp;
    fid_ent_t         *fep;
    fid_ent_t         *victimFep = NULL;
    bool               created=FALSE;
    struct coda_cred      *cred;
    fid_ent_t          dummy;
    char              *name=NULL;
    char              *path=NULL;
    ds_list_iter_t    *iter;
    struct stat        sbuf;
    uid_t              suid;
    gid_t              sgid;

    fp = &(in->cfs_rmdir.VFid);
    cred = &(in->ih.cred);
    assert((int)in->cfs_rmdir.name == VC_INSIZE(cfs_rmdir_in));
    name = (char*)in + (int)in->cfs_rmdir.name;

    if (verbose) {
	printf("Doing rmdir of (%s) in fid (%x.%x.%x)\n",
	       name, fp->Volume, fp->Vnode, fp->Unique);
    }

    /* Get the directory from which we are removing */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	printf("RMDIR: parent not directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Step 0: Check to ensure that removes don't happen to '.', '..', or '' */
    if ((!strcmp(name, "."))
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, ""))) 
    {
	printf("RMDIR: remove of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    path = fid_fullname(fep);
    /* Step 1: find the child */
    if (!child_exists(path,name)) {
	printf("RMDIR: child didn't exist!\n");
	out->oh.result = ENOENT;
	goto exit;
    }

    /* Do we know about the child yet? */
    iter = ds_list_iter_create(fep->kids);
    while ((victimFep = ds_list_iter_next(iter)) != NULL) {
	if (!strcmp(victimFep->name, name)) {
	    break;
	}
    }
    ds_list_iter_destroy(iter);
    
    if (!victimFep) {
	/* 
	 * We're going to create a victimFep as a temporary; this is
	 * only to make the code simpler.  I know that it isn't
	 * that efficient, but...
	 */
	victimFep = fid_create(name, fep);
	created = TRUE;
    }

    /* 
     * Get the path for the rmdir call.  Ensure it is a regular file.
     * We are *not* allowed to unlink anything else here.
     */
    path = fid_fullname(victimFep);
    if (lstat(path,&sbuf)) {
	out->oh.result = errno;
	goto exit;
    }
    switch(sbuf.st_mode & S_IFMT) {
    case S_IFREG:
	printf("RMDIR: trying to remove a file!\n");
	out->oh.result = EINVAL;
	goto exit;
	break;
    case S_IFLNK:
	printf("RMDIR: trying to remove a symlink!\n");
	out->oh.result = EINVAL;
	goto exit;
	break;
    case S_IFDIR:
	/* OK */
	break;
    case S_IFSOCK:               /* Can't be any of these */
    case S_IFIFO:
    case S_IFCHR:
    case S_IFBLK:
	printf("RMDIR: trying to remove an esoteric!\n");
	out->oh.result = EINVAL;
	goto exit;
	break;
    default:
	assert(0);
	break;
    }


    /* Do the rmdir. */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    if (rmdir(path)) {
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    }
    
    /* reset our effictive credentials */
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* We are now doomed to succeed */
    /* If the victimFep was already known, remove it */
    if (created == FALSE) {
	ds_list_remove(fep->kids, victimFep);
	ds_hash_remove(FidTab, victimFep);
    }
    out->oh.result = 0;
    
 exit:
    if (!out->oh.result) {
	/* If we succeeded, reclaim this fid */
	if (victimFep) free(victimFep);
    }
    if (path) free(path);
    *reply = VC_OUT_NO_DATA;
    return;
}

/* Phase 3 - esoterics */

void
DoReadlink(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid            *fp;
    fid_ent_t          *fep;
    fid_ent_t           dummy;
    struct coda_cred       *cred;
    char               *path;
    int                *count;
    uid_t               suid;
    gid_t               sgid;

    fp = &(in->cfs_readlink.VFid);
    count = &(out->cfs_readlink.count);
    cred = &(in->ih.cred);

    if (verbose) {
	printf("Doing readlink for fid (%x.%x.%x)\n",
	       fp->Volume, fp->Vnode, fp->Unique);
    }
    
    /* Grab the fid to readlink */
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VLNK) {
	printf("Ack!  Trying to readlink something not a symlink!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }
    /* Get the full pathname */
    path = fid_fullname(fep);
    /* Do the readlink */

    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    out->cfs_readlink.data = (char*)VC_OUTSIZE(cfs_readlink_out);
    *count = readlink(path,
		      (char*)out+(int)out->cfs_readlink.data,
		      VC_MAXDATASIZE-1);

    assert(!seteuid(suid));
    assert(!setgid(sgid));

    if (*count < 0) {
	printf("READLINK: readlink of %s failed (%s)\n",
	       path, strerror(errno));
	out->oh.result = errno;
	*count = 0;
	goto exit;
    }

    /* We're done. */
    out->oh.result = 0;
 exit:
    if (path) free(path);
    *reply = VC_OUTSIZE(cfs_readlink_out) + *count;
    return;
}

void
DoIoctl(union inputArgs *in, union outputArgs *out, int *reply)
{
    out->oh.result = EOPNOTSUPP;
    return;
}

void
DoLink(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid         *dfp;
    fid_ent_t       *dfep;
    ViceFid         *tfp;
    fid_ent_t       *tfep;
    fid_ent_t        dummy;
    fid_ent_t       *lfep=NULL;
    struct coda_cred    *cred;
    char            *name;
    char            *lpath=NULL;
    char            *tpath=NULL;
    uid_t            suid;
    gid_t            sgid;

    dfp = &(in->cfs_link.destFid);
    tfp = &(in->cfs_link.sourceFid);
    cred = &(in->ih.cred);
    assert((int)in->cfs_link.tname == VC_INSIZE(cfs_link_in));
    name = (char*)in + (int)in->cfs_link.tname;
    
    if (verbose) {
	printf("Doing hard link to fid (%x.%x.%x) in fid (%x.%x.%x)",
	       tfp->Volume, tfp->Vnode, tfp->Unique,
	       dfp->Volume, dfp->Vnode, dfp->Unique);
	printf(" with name %s\n", name);
    }


    /* 
     * For the moment, we'll place the same restrictions on links in
     * potemkin as we do in real Coda; hard links can only be to
     * things in the same directory.  This is more restrictive than is
     * strictly necessary, but so what?  Plus, Coda allows no hard
     * links to directories by anyone, so just in case the person
     * running the process making this request is root, don't let root
     * do that! 
     */

    /* Get the two fep's */
    dummy.fid = *tfp;
    assert((tfep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    dummy.fid = *dfp;
    assert((dfep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);

    /* Is dfp a directory? */
    if (dfep->type != C_VDIR) {
	printf("Ack!  Trying to hardlink in a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Is tfp *not* a directory? */
    if (tfep->type == C_VDIR) {
	printf("Ack!  Trying to hardlink to a directory!\n");
	out->oh.result = EPERM;
	goto exit;
    }

    /* Is tfp a child of dfp? */
    if (tfep->parent != dfep) {
	printf("Ack!  Cross directory hardlink!\n");
	out->oh.result = EXDEV;
	goto exit;
    }
	
    
    /* Don't allow any creation of '.', '..', ''; they already must exist. */
    if ((!strcmp(name, ".")) 
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, "")))
    {
	printf("CREATE: create of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    /* Okay: create a new fid entry for the link, and get it's path */
    lfep = fid_create(name, dfep);
    lpath = fid_fullname(lfep);
    tpath = fid_fullname(tfep);

    /* Do the link */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    if (link(tpath,lpath)) {
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    }

    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* Link created.  We're in good shape */
    /* Touch the file.  Ignore failure */
    utimes(lpath,NULL);

    /* 
     * Question: should we represent this as an inc'ed count?  I don't
     * think so.  At least, not for now, and I don't think it'll
     * matter 
     */
    out->oh.result = 0;
    lfep->type = tfep->type;  /* Whatever we linked to. */
    assert(ds_hash_insert(FidTab, lfep));
    assert(ds_list_insert(dfep->kids, lfep));
    
 exit:
    /* If the link failed, and we created a fid entry for the link */
    if (out->oh.result) {
	if (lfep) free(lfep);
    }
    if (lpath) free(lpath);
    if (tpath) free(tpath);
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoSymlink(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid        *fp;
    fid_ent_t      *fep;
    fid_ent_t      *newFep;
    fid_ent_t       dummy;
    struct coda_vattr   *attr;
    struct coda_cred   *cred;
    char           *name;
    char           *path=NULL;
    char           *contents;
    uid_t           suid;
    gid_t           sgid;

    fp = &(in->cfs_symlink.VFid);
    attr = &(in->cfs_symlink.attr);
    contents = (char*)in + (int)(in->cfs_symlink.srcname);
    name = (char*)in + (int)(in->cfs_symlink.tname);
    cred = &(in->ih.cred);

    if (verbose) {
	printf("Trying to create symlink %s in (%x.%x.%x) to %s\n",
	       name, fp->Volume, fp->Vnode, fp->Unique, contents);
    }
	
    dummy.fid = *fp;
    assert((fep = (fid_ent_t *) ds_hash_member(FidTab, &dummy)) != NULL);
    if (fep->type != C_VDIR) {
	printf("Ack!  Trying to symlink in a non-directory!\n");
	out->oh.result = ENOTDIR;
	goto exit;
    }

    /* Don't allow any creation of '.', '..', ''; they already must exist. */
    if ((!strcmp(name, ".")) 
	|| (!strcmp(name, ".."))
	|| (!strcmp(name, "")))
    {
	printf("CREATE: create of '.', '..' or ''\n");
	out->oh.result = EINVAL;
	goto exit;
    }
    
    /* Don't allow names of the form @XXXXXXXX.XXXXXXXX.XXXXXXXX */
    if ((strlen(name) == 27) 
	&& (name[0] == '@') 
	&& (name[9] == '.') 
	&& (name[18] == '.'))
    {
	out->oh.result = EINVAL;
	goto exit;
    }

    /* Create a temporary fid for this entry. */
    /* 
     * We won't look to see if one exists already, since if it does
     * the symlink will fail, and we won't enter it.
     */
    newFep = fid_create(name, fep);
    path = fid_fullname(newFep);
    
    /* Set up credentials */
    suid = getuid();
    sgid = getgid();
    assert(!setgid(cred->cr_gid));
    assert(!seteuid(cred->cr_uid));

    if (symlink(contents, path)) {
	out->oh.result = errno;
	assert(!seteuid(suid));
	assert(!setgid(sgid));
	goto exit;
    }	
    
    assert(!seteuid(suid));
    assert(!setgid(sgid));

    /* We know it's a symlink */
    newFep->type = C_VLNK;

    /* We're going to succeed.  Enter the fid, and set return value */
    out->oh.result = 0;
    assert(ds_hash_insert(FidTab, newFep));
    assert(ds_list_insert(fep->kids, newFep));

 exit:
    /* Toast the new vnode if we have an error */
    if (out->oh.result && newFep) free(newFep);
    if (path) free(path);
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoFsync(union inputArgs *in, union outputArgs *out, int *reply)
{
    ViceFid  *fp;
    
    fp = &(in->cfs_fsync.VFid);

    /* Fsync always succeeds, for now */
    if (verbose) {
	printf("Trival fsync for fid (%x.%x.%x)\n", fp->Volume,
	       fp->Vnode, fp->Unique);
	fflush(stdout);
    }
    out->oh.result = 0;
    *reply = VC_OUT_NO_DATA;
    return;
}

void
DoVget(union inputArgs *in, union outputArgs *out, int *reply)
{
    out->oh.result = EOPNOTSUPP;
    return;
}

/*************************************************** Dispatch */
int
Dispatch(union inputArgs *in, union outputArgs *out, int *reply) {
    out->oh.opcode = in->ih.opcode;
    out->oh.unique = in->ih.unique;

    if (verbose) {
	fprintf(stdout,"Opcode %d\tUniqe %d\n", in->ih.opcode, in->ih.unique);
    }
    fflush(stdout);

    switch(in->ih.opcode) {
    case CFS_ROOT:
	DoRoot(in,out,reply);
	break;
    case CFS_OPEN:
	DoOpen(in,out,reply);
	break;
    case CFS_CLOSE:
	DoClose(in,out,reply);
	break;
    case CFS_ACCESS:
	DoAccess(in,out,reply);
	break;
    case CFS_LOOKUP:
	DoLookup(in,out,reply);
	break;
    case CFS_GETATTR:
	DoGetattr(in,out,reply);
	break;
    case CFS_CREATE:
	DoCreate(in,out,reply);
	break;
    case CFS_REMOVE:
	DoRemove(in,out,reply);
	break;
    case CFS_SETATTR:
	DoSetattr(in,out,reply);
	break;
    case CFS_MKDIR:
	DoMkdir(in,out,reply);
	break;
    case CFS_RMDIR:
	DoRmdir(in,out,reply);
	break;
    case CFS_READLINK:
	DoReadlink(in,out,reply);
	break;
    case CFS_SYMLINK:
	DoSymlink(in,out,reply);
	break;
    case CFS_LINK:
	DoLink(in,out,reply);
	break;
    case CFS_RENAME:
	DoRename(in,out,reply);
	break;
    case CFS_FSYNC:
	DoFsync(in,out,reply);
	break;
    default:
	out->oh.result = EOPNOTSUPP;
	fprintf(stderr,"** Not Supported **");
	fflush(stderr);
	*reply = VC_OUT_NO_DATA;
	break;
    }

    return out->oh.result;
}

/*************************************************** Service */
void
Service()
{
    struct timeval     to;
    fd_set             readfds;
    fd_set             writefds;
    fd_set             exceptfds;
    int                nfds;
    int                rc;
    int                reply_size;
    char               inbuf[VC_MAXMSGSIZE];
    char               outbuf[VC_MAXMSGSIZE];
    union inputArgs  *in;
    union outputArgs *out;


    in = (union inputArgs *) inbuf;
    out = (union outputArgs *) outbuf;
    
    while (1) {
        /* Set up arguments for the selects */
        nfds = KernFD+1;
	to.tv_sec = Interval;
	to.tv_usec = 0;
	/* Select on the kernel fid.  Wait at most Interval secs */
	FD_ZERO(&readfds);
	FD_SET(KernFD, &readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	rc = select(nfds, &readfds, &writefds, &exceptfds, &to);
	if (rc == 0) {
	    fprintf(stderr,"Interval - no message\n");
	    fflush(stderr);
	    continue;
	} else if (rc < 0) {
	    if (errno == EINTR) {
		/* I think EINTR won't come back if we are ctrl-c'd.. */
#if 0
		fprintf(stderr,"We were interrupted\n");
		fflush(stderr);
		break;
#else
		continue;
#endif

	    } else {
		perror("select'ing");
		exit(-1);
	    }
	}

	/* Read what we can... */
	assert((rc = read(KernFD, inbuf, (int) VC_MAXMSGSIZE)) >= 0);
	if (rc < VC_IN_NO_DATA) {
	    fprintf(stderr,"Message fragment: size %d --",rc);
	    perror(NULL);
	    continue;
	}

	/* Dispatch the request */
	reply_size = VC_OUT_NO_DATA;
	rc = Dispatch(in, out, &reply_size);
	if (verbose) {
	    if (rc != 0) {
		errno = rc;
		perror("Dispatch returns error");
	    }
	}

	/* Write out the result */
	assert((rc = write(KernFD, outbuf, reply_size)) >= 0);
	if (rc < reply_size) {
	    fprintf(stderr,"Wrote fragment %d/%d --", rc, reply_size);
	    perror(NULL);
	    continue;
	}
    }
}

/*************************************************** main */
int
main(int argc, char *argv[])
{
    printf("User id is: %d\n",getuid());

    ParseArgs(argc, argv);
    Setup();
    Service();

    return 0;
}

#ifdef __linux__
void
coda_iattr_to_vattr(struct iattr *iattr, struct coda_vattr *vattr)
{
        umode_t mode;
        unsigned int valid;

        /* clean out */        
        vattr->va_mode = (umode_t) -1;
        vattr->va_uid = (vuid_t) -1; 
        vattr->va_gid = (vgid_t) -1;
        vattr->va_size = (off_t) -1;
	vattr->va_atime.tv_sec = (time_t) -1;
        vattr->va_mtime.tv_sec  = (time_t) -1;
	vattr->va_ctime.tv_sec  = (time_t) -1;
	vattr->va_atime.tv_nsec =  (time_t) -1;
        vattr->va_mtime.tv_nsec = (time_t) -1;
	vattr->va_ctime.tv_nsec = (time_t) -1;
        vattr->va_type = C_VNON;
	vattr->va_fileid = (long)-1;
	vattr->va_gen = (long)-1;
	vattr->va_bytes = (long)-1;
	vattr->va_nlink = (short)-1;
	vattr->va_blocksize = (long)-1;
	vattr->va_rdev = (dev_t)-1;
        vattr->va_flags = 0;

        /* determine the type */
        mode = iattr->ia_mode;
                if ( S_ISDIR(mode) ) {
                vattr->va_type = C_VDIR; 
        } else if ( S_ISREG(mode) ) {
                vattr->va_type = C_VREG;
        } else if ( S_ISLNK(mode) ) {
                vattr->va_type = C_VLNK;
        } else {
                /* don't do others */
                vattr->va_type = C_VNON;
        }
        
        /* set those vattrs that need change */
        valid = iattr->ia_valid;
        if ( valid & ATTR_MODE ) 
                vattr->va_mode = iattr->ia_mode;
        if ( valid & ATTR_UID )
                vattr->va_uid = (vuid_t) iattr->ia_uid;
        if ( valid & ATTR_GID ) 
                vattr->va_gid = (vgid_t) iattr->ia_gid;
        if ( valid & ATTR_SIZE )
                vattr->va_size = iattr->ia_size;
	if ( valid & ATTR_ATIME )
                vattr->va_atime.tv_sec = iattr->ia_atime;
        if ( valid & ATTR_MTIME )
                vattr->va_mtime.tv_sec = iattr->ia_mtime;
        if ( valid & ATTR_CTIME )
                vattr->va_ctime.tv_sec = iattr->ia_ctime;
        
}
#endif
