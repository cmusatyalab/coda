/*
 * Cache operations for Coda.
 * For Linux 2.1: (C) 1997 Carnegie Mellon University
 *
 * Carnegie Mellon encourages users of this code to contribute improvements
 * to the Coda project. Contact Peter Braam <coda@cs.cmu.edu>.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/list.h>

#include <linux/coda.h>
#include <linux/coda_linux.h>
#include <linux/coda_psdev.h>
#include <linux/coda_cnode.h>
#include <linux/coda_cache.h>

/* Keep various stats */
struct cfsnc_statistics cfsnc_stat;


/* we need to call INIT_LIST_HEAD on cnp->c_cnhead and sbi->sbi_cchead */

void coda_ccinsert(struct coda_cache *el, struct super_block *sb)
{
	struct coda_sb_info *sbi = coda_sbp(sb);
ENTRY;
	if ( !sbi ||  !el) {
		printk("coda_ccinsert: NULL sbi or el!\n");
		return ;
	}

	list_add(&el->cc_cclist, &sbi->sbi_cchead);
}

void coda_cninsert(struct coda_cache *el, struct cnode *cnp)
{
ENTRY;
	if ( !cnp ||  !el) {
		printk("coda_cninsert: NULL cnp or el!\n");
		return ;
	}
	list_add(&el->cc_cnlist, &cnp->c_cnhead);
}

void coda_ccremove(struct coda_cache *el)
{
ENTRY;
	list_del(&el->cc_cclist);
}

void coda_cnremove(struct coda_cache *el)
{
ENTRY;
	list_del(&el->cc_cnlist);
}


void coda_cache_create(struct inode *inode, int mask)
{
	struct cnode *cnp = ITOC(inode);
	struct super_block *sb = inode->i_sb;
	struct coda_cache *cc = NULL;
ENTRY;
	CODA_ALLOC(cc, struct coda_cache *, sizeof(*cc));

	if ( !cc ) {
		printk("Out of memory in coda_cache_enter!\n");
		return;
	}
	coda_load_creds(&cc->cc_cred);
	cc->cc_mask = mask;
	coda_cninsert(cc, cnp);
	coda_ccinsert(cc, sb);
}

struct coda_cache * coda_cache_find(struct inode *inode)
{
	struct cnode *cnp = ITOC(inode);
	struct list_head *lh, *le;
	struct coda_cache *cc = NULL;
	
	le = lh = &cnp->c_cnhead;
	while( (le = le->next ) != lh )  {
		/* compare name and creds */
		cc = list_entry(le, struct coda_cache, cc_cnlist);
		if ( !coda_cred_ok(&cc->cc_cred) )
			continue;
		CDEBUG(D_CACHE, "HIT for ino %ld\n", inode->i_ino );
		return cc; /* cache hit */
	}
		return NULL;
}

void coda_cache_enter(struct inode *inode, int mask)
{
	struct coda_cache *cc;

	cc = coda_cache_find(inode);

	if ( cc ) {
		cc->cc_mask |= mask;
	} else {
		coda_cache_create(inode, mask);
	}
}

void coda_cache_clear_cnp(struct cnode *cnp)
{
	struct list_head *lh, *le;
	struct coda_cache *cc;

	if ( !cnp ) {
		printk("coda_cache_cnp_clear: NULL cnode\n");
		return;
	}
	
	lh = le = &cnp->c_cnhead;
	while ( (le = le->next ) != lh ) {
		cc = list_entry(le, struct coda_cache, cc_cnlist);
		coda_cnremove(cc);
		coda_ccremove(cc);
		CODA_FREE(cc, sizeof(*cc));
	}
}

void coda_cache_clear_all(struct super_block *sb)
{
	struct list_head *lh, *le;
	struct coda_cache *cc;
	struct coda_sb_info *sbi = coda_sbp(sb);

	if ( !sbi ) {
		printk("coda_cache_clear_all: NULL sbi\n");
		return;
	}
	
	lh = le = &sbi->sbi_cchead;
	while ( (le = le->next ) != lh ) {
		cc = list_entry(le, struct coda_cache, cc_cclist);
		coda_cnremove(cc);
		coda_ccremove(cc);
		CODA_FREE(cc, sizeof(*cc));
	}
}

void coda_cache_clear_cred(struct super_block *sb, struct coda_cred *cred)
{
	struct list_head *lh, *le;
	struct coda_cache *cc;
	struct coda_sb_info *sbi = coda_sbp(sb);

	if ( !sbi ) {
		printk("coda_cache_clear_all: NULL sbi\n");
		return;
	}
	
	lh = le = &sbi->sbi_cchead;
	while ( (le = le->next ) != lh ) {
		cc = list_entry(le, struct coda_cache, cc_cclist);
		if ( coda_cred_eq(&cc->cc_cred, cred)) {
			coda_cnremove(cc);
			coda_ccremove(cc);
			CODA_FREE(cc, sizeof(*cc));
		}
	}
}
		

int coda_cache_check(struct inode *inode, int mask)
{
	struct cnode *cnp = ITOC(inode);
	struct list_head *lh, *le;
	struct coda_cache *cc = NULL;
	
	le = lh = &cnp->c_cnhead;
	while( (le = le->next ) != lh )  {
		/* compare name and creds */
		cc = list_entry(le, struct coda_cache, cc_cnlist);
		if ( (cc->cc_mask & mask) != mask ) 
			continue; 
		if ( !coda_cred_ok(&cc->cc_cred) )
			continue;
		CDEBUG(D_CACHE, "HIT for ino %ld\n", inode->i_ino );
		return 1; /* cache hit */
	}
		CDEBUG(D_CACHE, "MISS for ino %ld\n", inode->i_ino );
		return 0;
}


/*   DENTRY related stuff */

/* when the dentry count falls to 0 this is called. If Venus has
   asked for it to be flushed, we take it out of the dentry hash 
   table with d_drop */

static void coda_flag_children(struct dentry *parent)
{
	struct list_head *child;
	struct cnode *cnp;
	struct dentry *de;
	char str[50];

	child = parent->d_subdirs.next;
	while ( child != &parent->d_subdirs ) {
		de = list_entry(child, struct dentry, d_child);
		cnp = ITOC(de->d_inode);
		if (cnp) 
			cnp->c_flags |= C_ZAPFID;
		CDEBUG(D_CACHE, "ZAPFID for %s\n", coda_f2s(&cnp->c_fid, str));
		
		child = child->next;
	}
	return; 
}

/* flag dentry and possibly  children of a dentry with C_ZAPFID */
void  coda_dentry_delete(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct cnode *cnp = NULL;
	ENTRY;

	if (inode) { 
		cnp = ITOC(inode);
		if ( cnp ) 
			CHECK_CNODE(cnp);
	} else {
		CDEBUG(D_CACHE, "No inode for dentry_delete!\n");
		return;
	}

	
	if ( !cnp ) {
		printk("No cnode for dentry_delete!\n");
		return;
	}

	if ( cnp->c_flags & (C_ZAPFID | C_ZAPDIR) )
		d_drop(dentry);
	if ( (cnp->c_flags & C_ZAPDIR) && S_ISDIR(inode->i_mode) ) {
		coda_flag_children(dentry);
	}
	return;
}

static void coda_zap_cnode(struct cnode *cnp, int flags)
{
	cnp->c_flags |= flags;
	coda_cache_clear_cnp(cnp);
}



/* the dache will notice the flags and drop entries (possibly with
   children) the moment they are no longer in use  */
void coda_zapfid(struct ViceFid *fid, struct super_block *sb, int flag)
{
	struct inode *inode = NULL;
	struct cnode *cnp;

	ENTRY;
 
	if ( !sb ) {
		printk("coda_zapfid: no sb!\n");
		return;
	}

	if ( !fid ) {
		printk("coda_zapfid: no fid!\n");
		return;
	}

	if ( coda_fid_is_volroot(fid) ) {
		struct list_head *lh, *le;
		struct coda_sb_info *sbi = coda_sbp(sb);
		le = lh = &sbi->sbi_volroothead;
		while ( (le = le->next) != lh ) {
			cnp = list_entry(le, struct cnode, c_volrootlist);
			if ( cnp->c_fid.Volume == fid->Volume) 
				coda_zap_cnode(cnp, flag);
		}
		return;
	}


	inode = coda_fid_to_inode(fid, sb);
	if ( !inode ) {
		CDEBUG(D_CACHE, "coda_zapfid: no inode!\n");
		return;
	}
	cnp = ITOC(inode);
	CHECK_CNODE(cnp);
	if ( !cnp ) {
		printk("coda_zapfid: no cnode!\n");
		return;
	}
	coda_zap_cnode(cnp, flag);
}
		

int
cfsnc_nc_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
        int len=0;
        off_t begin;
	
	/* 	cfsnc_gather_stats(); */

	/* this works as long as we are below 1024 characters! */    
	len += sprintf(buffer,"Coda minicache statistics\n\n");
	len += sprintf(buffer+len, "cfsnc_hits : %d\n", cfsnc_stat.hits);
	len += sprintf(buffer+len, "cfsnc_misses : %d\n", cfsnc_stat.misses);
	len += sprintf(buffer+len, "cfsnc_enters : %d\n", cfsnc_stat.enters);
	len += sprintf(buffer+len, "cfsnc_dbl_enters : %d\n", cfsnc_stat.dbl_enters);
	len += sprintf(buffer+len, "cfsnc_long_name_enters : %d\n", cfsnc_stat.long_name_enters);
	len += sprintf(buffer+len, "cfsnc_long_name_lookups : %d\n", cfsnc_stat.long_name_lookups);
	len += sprintf(buffer+len, "cfsnc_long_remove : %d\n", cfsnc_stat.long_remove);
	len += sprintf(buffer+len, "cfsnc_lru_rm : %d\n", cfsnc_stat.lru_rm);
	len += sprintf(buffer+len, "cfsnc_zapPfids : %d\n", cfsnc_stat.zapPfids);
	len += sprintf(buffer+len, "cfsnc_zapFids : %d\n", cfsnc_stat.zapFids);
	len += sprintf(buffer+len, "cfsnc_zapFile : %d\n", cfsnc_stat.zapFile);
	len += sprintf(buffer+len, "cfsnc_zapUsers : %d\n", cfsnc_stat.zapUsers);
	len += sprintf(buffer+len, "cfsnc_Flushes : %d\n", cfsnc_stat.Flushes);
	len += sprintf(buffer+len, "cfsnc_SumLen : %d\n", cfsnc_stat.Sum_bucket_len);
	len += sprintf(buffer+len, "cfsnc_Sum2Len : %d\n", cfsnc_stat.Sum2_bucket_len);
	len += sprintf(buffer+len,  "cfsnc_# 0 len : %d\n", cfsnc_stat.Num_zero_len);
	len += sprintf(buffer+len,  "cfsnc_MaxLen : %d\n", cfsnc_stat.Max_bucket_len);
	len += sprintf(buffer+len,  "cfsnc_SearchLen : %d\n", cfsnc_stat.Search_len);
       	begin =  offset;
       	*start = buffer + begin;
       	len -= begin;
	
        if(len>length)
                len = length;
	if (len< 0)
		len = 0;
        return len;
} 
