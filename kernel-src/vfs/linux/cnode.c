/* cnode related routines for the coda kernel code
   Peter Braam, Sep 1996.
   */


#include "cfs.h"
#include "cnode.h"
#include "super.h" 

extern int coda_debug;
extern int coda_print_entry;
struct cnode *coda_cnode_cache[ CNODE_CACHESIZE ];
struct cnode *coda_cnode_freelist = NULL;
int coda_cnode_reuse = 0;
int coda_cnode_new = 0;
int coda_cnode_active = 0;
int coda_ino = 0;

/* cnode.c */
void coda_flush(void);
struct cnode *coda_cnode_find(ViceFid *fid);
int coda_flush_cinode(struct cnode *cnp);
struct inode *coda_cnode_make(ViceFid *fid, struct super_block *sb);
struct cnode *coda_cnode_alloc(void);
struct inode *coda_cinode_make(ViceFid *fid, struct super_block *sb);
struct coda_inode *coda_cinode_alloc(void);
void coda_cinode_free(struct coda_inode *);
void coda_cnode_save(struct cnode *cnp);
void coda_cnode_remove(struct cnode *cnp);
void coda_cnode_free(struct cnode *cnp);
int coda_cnode_flush(struct cnode *cnp);
int coda_fetch_inode(struct inode *inode, struct vattr *attr);

/* flush the entire cnode cache */
void
coda_flush()
{
        int hash;
        struct cnode *cnp;
        
        for (hash = 0; hash < CNODE_CACHESIZE ; hash++) {
                for (cnp = coda_cnode_cache[hash]; cnp != NULL; 
                     cnp = CNODE_NEXT(cnp)) {  
                        coda_cnode_flush(cnp);
                }
        }
}

/* here we want to set the inode to an invalid mode, as a result of
   either a dying Venus or a broken promise.
   This may crash an executing program if a page fault occurs */
int
coda_cnode_flush(struct cnode *cnp)
{
        cnp->c_device = 0;
        cnp->c_inode = 0;

        /* no idea how to remove a "stale" inode under Linux XXXX */
        cnp->c_vnode->i_dev=0;
        
        return 0;
}


/* make a cnode for given ViceFid and super block, 
   ask Venus for attributes */
struct inode *
coda_cnode_make(ViceFid *fid, struct super_block *sb)
{
        struct cnode *cnp;
        struct inode *result;

        ENTRY; 

        cnp = coda_cnode_alloc();
        if (!cnp) {
                DEBUG("coda_make_cnode:coda_cnode_alloc failed");
                return NULL;
        }
  
        cnp->c_fid = *fid;
        cnp->c_magic = CODA_CNODE_MAGIC;

        /* note that iget calls coda_read_inode which links 
           the cnode and inode 
           */
        DEBUG("calling iget for fid (%ld,%ld,%ld), ino %ld.\n", 
              fid->Volume, fid->Vnode, fid->Unique, (u_long) cnp);
        coda_ino++;
        /* the following inode number is certainly not taken yet 
         *  since we will not be interrupted until read_inode sets it up right
         */
        result = iget(sb, ((int) cnp) + coda_ino++);
        if (!result) {
                DEBUG(("coda_cnode_make: iget inode failed\n"));
                if (cnp) {
                        coda_cnode_free(cnp);
                }
                return NULL;
        }
        cnp->c_vnode = result;
        coda_cnode_save(cnp);
EXIT;
        return result;
}

/* return pointer to new coda_inode */
struct coda_inode *
coda_cinode_alloc(void)
{
        struct coda_inode *result = NULL;

        result = (struct coda_inode *) kmalloc ( sizeof(struct coda_inode), 
                                                 GFP_KERNEL);
        if ( !result ) {
                DEBUG("kmalloc returned NULL.\n");
                return result;
        }
        memset(result, 0, (int) sizeof(struct coda_inode));

        return result;
}

void 
coda_cinode_free(struct coda_inode *cinode)
{
        kfree(cinode);
}
              

struct inode *
coda_cinode_make(ViceFid *fid, struct super_block *sb)
{
        struct cnode *cnp;
        struct coda_inode *cinode;
        struct inode *result;
        int error;
        struct vattr attr;
        
        ENTRY;
        cinode = coda_cinode_alloc();
        if ( !cinode ) {
                DEBUG("coda_cnode_alloc failed.\n");
                return NULL;
        }

        cnp = &(cinode->ci_cnode);
        cnp->c_fid = *fid;
        cnp->c_magic = CODA_CNODE_MAGIC;

        /* 
         * We get inode numbers from Venus -- see venus source
         * 
         * (getcwd is a good example) 
         */
        
        error = coda_getvattr(fid, &attr, coda_sbp(sb));
        
        if ( error ) {
                /* clear cinode deallocate memory XXXXX */
                printk("coda_cinode_make: coda_getvattr returned %d\n", error);
                return NULL;
        }

        result = iget(sb, attr.va_fileid);

        if ( !result ) {
                printk("coda_cinode_make: iget failed\n");
                if (cinode) coda_cinode_free(cinode);
                return NULL;
        }

        cnp->c_vnode = result;
        
        if ( ! result->u.generic_ip ) {
                result->u.generic_ip = (void *) cinode;
        } else {
                coda_cinode_free(cinode);
        }
        
        error = coda_fetch_inode(result, &attr);
        if ( error ) {
                DEBUG("coda_fetch_inode returned %d\n", error);
                return NULL;
        }


        EXIT;
        return result;
}

/* return pointer to unused cnode */
struct cnode *
coda_cnode_alloc()
{
        struct cnode *cnp;

        if (coda_cnode_freelist) {
                cnp = coda_cnode_freelist;
                coda_cnode_freelist = CNODE_NEXT(cnp);
                coda_cnode_reuse++;
        } else {
                cnp = (struct cnode *) kmalloc(sizeof(struct cnode), GFP_KERNEL);
                
                if ( !cnp ) {
                        DEBUG(("coda: coda_cnode_alloc: kmalloc failed\n"));
                        return NULL;
                } 
                coda_cnode_new++;
                memset(cnp, 0, (int)sizeof(struct cnode));
        }
        /*  printk("coda_cnode_alloc: address of cnode: %d\n", (int) cnp); */
        return cnp;
}
      
/* find cnode for given ViceFid */
struct cnode *
coda_cnode_find(ViceFid *fid)
{
        struct cnode *cnp;
        
        cnp = coda_cnode_cache[cnode_hash(fid)];

        while (cnp) {
                if ( (cnp->c_fid.Vnode == fid->Vnode) &&
                     (cnp->c_fid.Volume == fid->Volume) &&
                     (cnp->c_fid.Unique == fid->Unique) &&
                     (!IS_DYING(cnp))) {
                        coda_cnode_active++;
                        return(cnp);
                }
                cnp = CNODE_NEXT(cnp);
        }
        return(NULL);
}

/* put cnode on freelist */

void 
coda_cnode_free(struct cnode *cnp)
{
        CNODE_NEXT(cnp) = coda_cnode_freelist;
        coda_cnode_freelist = cnp;
}



/* put cnode in hash table */
void 
coda_cnode_save(struct cnode *cnp)
{
        CNODE_NEXT(cnp) = coda_cnode_cache[cnode_hash(&cnp->c_fid)];
        coda_cnode_cache[cnode_hash(&cnp->c_fid)] = cnp;
}


/* remove cnode from the hash table */
void
coda_cnode_remove(struct cnode *cnp)
{
        struct cnode *ptr, *prevptr  = NULL;
        int hash = cnode_hash(&cnp->c_fid);
        
        ptr = coda_cnode_cache[hash];
        while ( ptr != NULL ) {
                if ( ptr == cnp ) {
                        if ( prevptr == NULL ) {
                                coda_cnode_cache[hash] = CNODE_NEXT(cnp);
                        } else {
                                CNODE_NEXT(prevptr) = CNODE_NEXT(cnp);
                        }
                        CNODE_NEXT(cnp) = NULL;
                        return;
                }
                prevptr = ptr;
                ptr = CNODE_NEXT(ptr);
        }
        printk("coda: coda_cnode_remove: trying to remove non existent cnode!");
}

/*
 * For easier use of the filesystem, some inodes may be orphaned when Venus
 * dies. Certain operations attempt to go through, without propagating
 * orphan-ness.  This function gets a new inode for the file from the 
 * current run of Venus.
 */





