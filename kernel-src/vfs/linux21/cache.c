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
#include <linux/coda_fs_i.h>
#include <linux/coda_cache.h>

static void coda_ccinsert(struct coda_cache *el, struct super_block *sb);
static void coda_cninsert(struct coda_cache *el, struct coda_inode_info *cii);
static void coda_ccremove(struct coda_cache *el);
static void coda_cnremove(struct coda_cache *el);
static void coda_cache_create(struct inode *inode, int mask);
static struct coda_cache * coda_cache_find(struct inode *inode);


/* insert a acl-cache entry in sb list */
static void coda_ccinsert(struct coda_cache *el, struct super_block *sb)
{
	struct coda_sb_info *sbi = coda_sbp(sb);
	ENTRY;
	if ( !sbi ||  !el) {
		printk("coda_ccinsert: NULL sbi or el!\n");
		return ;
	}

	list_add(&el->cc_cclist, &sbi->sbi_cchead);
}

/* insert a acl-cache entry in the inode list */
static void coda_cninsert(struct coda_cache *el, struct coda_inode_info *cii)
{
	ENTRY;
	if ( !cii ||  !el) {
		printk("coda_cninsert: NULL cii or el!\n");
		return ;
	}
	list_add(&el->cc_cnlist, &cii->c_cnhead);
}

/* remove a cache entry from the superblock list */
static void coda_ccremove(struct coda_cache *el)
{
	ENTRY;
        if (el->cc_cclist.next && el->cc_cclist.prev)
	        list_del(&el->cc_cclist);
	else
		printk("coda_cnremove: trying to remove 0 entry!");
}

/* remove a cache entry from the inode's list */
static void coda_cnremove(struct coda_cache *el)
{
	ENTRY;
	if (el->cc_cnlist.next && el->cc_cnlist.prev)
		list_del(&el->cc_cnlist);
	else
		printk("coda_cnremove: trying to remove 0 entry!");
}

/* create a new cache entry and enlist it */
static void coda_cache_create(struct inode *inode, int mask)
{
	struct coda_inode_info *cii = ITOC(inode);
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
	coda_cninsert(cc, cii);
	coda_ccinsert(cc, sb);
}

/* see if there is a match for the current 
   credentials already */
static struct coda_cache * coda_cache_find(struct inode *inode)
{
	struct coda_inode_info *cii = ITOC(inode);
	struct list_head *lh, *le;
	struct coda_cache *cc = NULL;
	
	le = lh = &cii->c_cnhead;
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

/* create or extend an acl cache hit */
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

/* remove all cached acl matches from an inode */
void coda_cache_clear_inode(struct inode *inode)
{
	struct list_head *lh, *le;
	struct coda_inode_info *cii;
	struct coda_cache *cc;
	ENTRY;

	if ( !inode ) {
		CDEBUG(D_CACHE, "coda_cache_clear_inode: NULL inode\n");
		return;
	}
	cii = ITOC(inode);
	
	lh = le = &cii->c_cnhead;
	while ( (le = le->next ) != lh ) {
		cc = list_entry(le, struct coda_cache, cc_cnlist);
		coda_cnremove(cc);
		coda_ccremove(cc);
		CODA_FREE(cc, sizeof(*cc));
	}
}

/* remove all acl caches */
void coda_cache_clear_all(struct super_block *sb)
{
	struct list_head *lh, *le;
	struct coda_cache *cc;
	struct coda_sb_info *sbi = coda_sbp(sb);

	if ( !sbi ) {
		printk("coda_cache_clear_all: NULL sbi\n");
		return;
	}
	
	if ( list_empty(&sbi->sbi_cchead) )
		return;

	lh = le = &sbi->sbi_cchead;
	while ( (le = le->next ) != lh ) {
		cc = list_entry(le, struct coda_cache, cc_cclist);
		coda_cnremove(cc);
		coda_ccremove(cc);
		CODA_FREE(cc, sizeof(*cc));
	}
}

/* remove all acl caches for a principal */
void coda_cache_clear_cred(struct super_block *sb, struct coda_cred *cred)
{
	struct list_head *lh, *le;
	struct coda_cache *cc;
	struct coda_sb_info *sbi = coda_sbp(sb);

	if ( !sbi ) {
		printk("coda_cache_clear_all: NULL sbi\n");
		return;
	}
	
	if (list_empty(&sbi->sbi_cchead))
		return;

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


/* check if the mask has been matched against the acl
   already */
int coda_cache_check(struct inode *inode, int mask)
{
	struct coda_inode_info *cii = ITOC(inode);
	struct list_head *lh, *le;
	struct coda_cache *cc = NULL;
	
	le = lh = &cii->c_cnhead;
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


/*   DCACHE & ZAPPING related stuff */

/* the following routines set flags in the inodes. They are 
   detected by:
   - a dentry method: coda_dentry_revalidate (for lookups)
     if the flag is C_PURGE
   - an inode method coda_revalidate (for attributes) if the 
     flag is C_ATTR
*/
static void coda_flag_children(struct dentry *parent, int flag)
{
	struct list_head *child;
	struct dentry *de;

	child = parent->d_subdirs.next;
	while ( child != &parent->d_subdirs ) {
		de = list_entry(child, struct dentry, d_child);
		coda_flag_inode(de->d_inode, flag);
		CDEBUG(D_CACHE, "%d for %*s/%*s\n", flag, 
		       de->d_name.len, de->d_name.name, 
		       de->d_parent->d_name.len, de->d_parent->d_name.name);
		child = child->next;
		if ( !de->d_inode )
			d_drop(de); 
	}
	return; 
}


void coda_flag_alias_children(struct inode *inode, int flag)
{
	struct list_head *alias;
	struct dentry *alias_de;

	if ( !inode ) 
		return; 
	alias = inode->i_dentry.next; 
	while ( alias != &inode->i_dentry ) {
		alias_de = list_entry(alias, struct dentry, d_alias);
		if ( !alias_de ) {
			printk("Null alias list for inode %ld\n", inode->i_ino);
			return;
		}
		coda_flag_children(alias_de, flag);
		alias= alias->next;
	}
}

void coda_flag_inode(struct inode *inode, int flag)
{
	struct coda_inode_info *cii;

	if ( !inode ) {
		CDEBUG(D_CACHE, " no inode!\n");
		return;
	}
	cii = ITOC(inode);
	cii->c_flags |= flag;
}		





