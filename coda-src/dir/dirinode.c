#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>

#include <lwp.h>
#include <lock.h>
#include <cfs/coda.h>
#include <dllist.h>
#include <util.h>
#include <rvmlib.h>
#include "codadir.h"

/* copy directory into contiguous directory header */
PDirHeader DI_DiToDh(PDirInode pdi)
{
	char *pdh;
	int i; 
	int size;

	size = DIR_PAGESIZE * DI_Pages(pdi);

	pdh = (char *) malloc(size);
	assert(pdh);

	for (i = 0; i < DIR_MAXPAGES; i++) {
		if (pdi->di_pages[i] == 0) 
			break;
		bcopy(pdi->di_pages[i], &pdh[i * DIR_PAGESIZE], DIR_PAGESIZE);
	}
	return (PDirHeader) pdh;
}

/* copy a dir handle to a directory inode; create latter if needed */
PDirInode DI_DhToDi(PDCEntry pdce, PDirInode pdi)
{
	PDirHandle pdh = DC_DC2DH(pdce);
	int pages;
	int i;
	PDirInode lpdi;

	DIR_intrans();

	assert(pdh);
	pages = DH_Length(pdh)/DIR_PAGESIZE;

	if (pdi == NULL) {
		lpdi = (PDirInode) rvmlib_rec_malloc(sizeof(*lpdi));
		assert(lpdi);
		bzero((char *)lpdi, sizeof(*lpdi));
	} else {
		lpdi = pdi;
	}

	rvmlib_set_range(lpdi, sizeof(*lpdi));
	lpdi->di_refcount = DC_Refcount(pdce);
	
	/* copy pages to the dir inode */
	for ( i=0 ; i<pages ; i++) {
		if ( lpdi->di_pages[i] == 0 ) {
			lpdi->di_pages[i] = rvmlib_rec_malloc(DIR_PAGESIZE);
			assert(lpdi->di_pages[i]); 
		}
		rvmlib_set_range(lpdi->di_pages[i], DIR_PAGESIZE);
		bcopy((const void *)DIR_Page(pdh->dh_vmdata, i), lpdi->di_pages[i],
		      DIR_PAGESIZE);
	}

	/* free pages which have disappeared */
	for ( i=pages ; i<DIR_MAXPAGES ; i++ ) {
		if (lpdi->di_pages[i])
			rvmlib_rec_free(lpdi->di_pages);
	}

	return lpdi; 
}

/* reduce the refcount of the directory, delete it when it falls to 0 */
void DI_Dec(PDirInode pdi)
{
	int rcount;
	int i;

	DIR_intrans();

	assert(pdi); 
	rcount = pdi->di_refcount;
	if (rcount == 1){
		/* Last vnode referencing directory inode - delete it */
		for (i = 0; i < DIR_MAXPAGES; i++)
			if (pdi->di_pages[i]){
				DLog(29, "Deleting page %d for directory", i);
				rvmlib_rec_free(pdi->di_pages[i]);
		}
		DLog(29, "Deleting inode ");
		rvmlib_rec_free((void *)pdi);
	} else {
		rcount--;
		RVMLIB_MODIFY(pdi->di_refcount, rcount);
	}
}

void DI_Inc(PDirInode pdi)
{
	int rcount;

	DIR_intrans();

        assert(pdi);
	rcount = pdi->di_refcount;
	rcount++;
	RVMLIB_MODIFY(pdi->di_refcount, rcount);
}

/* return the refcoount of a directory */
int DI_Count(PDirInode pdi)
{
        assert(pdi);
	return  pdi->di_refcount;
}

/* return the number of pages in a directory */
int DI_Pages(PDirInode pdi)
{
	int i = 0;
	assert(pdi);

	while( pdi->di_pages[i] && (i <= DIR_MAXPAGES)) 
		i++;
	
	/* check this guy is valid */
	assert(i< DIR_MAXPAGES);
	return i;
}

/* return a pointer to a page in a directory */
void *DI_Page(PDirInode pdi, int page)
{
	
	assert(pdi);
	assert(page>0 && page < DIR_MAXPAGES);

	return pdi->di_pages[page];
}


/* copies oldinode and its pages by first allocating a newinode */
void DI_Copy(PDirInode oldinode, PDirInode *newinode)
{
	int i;

	DIR_intrans();

	DLog(29, "Entering DI_Copy(%p , %p)", oldinode, newinode);
	assert(oldinode);

	*newinode = (PDirInode)rvmlib_rec_malloc(sizeof(**newinode));
	assert(*newinode);
	rvmlib_set_range(*newinode, sizeof(*newinode));

	bzero((void *)*newinode, sizeof(**newinode));
	for(i = 0; i < DIR_MAXPAGES; i++)
		if (oldinode->di_pages[i]){
			DLog(29, "CopyDirInode: Copying page %d", i);
			(*newinode)->di_pages[i] = rvmlib_rec_malloc(DIR_PAGESIZE);
			rvmlib_modify_bytes((*newinode)->di_pages[i], 
					    oldinode->di_pages[i], DIR_PAGESIZE);
		}
	(*newinode)->di_refcount = oldinode->di_refcount;
	return;
}

/* copies oldinode and its pages by first allocating a newinode */
void DI_VMCopy(PDirInode oldinode, PDirInode *newinode)
{
	int i;

	DLog(29, "Entering DI_Copy(%p , %p)", oldinode, newinode);
	assert(oldinode);

	*newinode = (PDirInode)malloc(sizeof(**newinode));
	assert(*newinode);

	bzero((void *)*newinode, sizeof(**newinode));
	for(i = 0; i < DIR_MAXPAGES; i++)
		if (oldinode->di_pages[i]){
			DLog(29, "CopyDirInode: Copying page %d", i);
			(*newinode)->di_pages[i] = malloc(DIR_PAGESIZE);
			bcopy((*newinode)->di_pages[i], oldinode->di_pages[i], 
			      DIR_PAGESIZE);
		}
	(*newinode)->di_refcount = oldinode->di_refcount;
	return;
}

/* reduce the refcount of a VM directory inode, 
   delete it when it falls to 0 */
void DI_VMDec(PDirInode pdi)
{
	int rcount;
	int i;

	assert(pdi); 
	rcount = pdi->di_refcount;
	if (rcount == 1){
		/* Last vnode referencing directory inode - delete it */
		for (i = 0; i < DIR_MAXPAGES; i++)
			if (pdi->di_pages[i]){
				DLog(29, "Deleting page %d for directory", i);
				free(pdi->di_pages[i]);
		}
		DLog(29, "Deleting inode ");
		free((void *)pdi);
	} else {
		rcount--;
	}
}

/* Free up a VM Directory Inode */
void DI_VMFree(PDirInode pdi)
{

	assert(pdi); 
	pdi->di_refcount = 1;
	DI_VMDec(pdi);
}
/* allocate a new directory inode in RVM */
PDirInode DI_New()
{
	PDirInode newinode;
	DIR_intrans();

	DLog(29, "Entering DI_New");

	newinode = (PDirInode)rvmlib_rec_malloc(sizeof(*newinode));
	assert(newinode);
	rvmlib_set_range(newinode, sizeof(newinode));
	bzero((void *)newinode, sizeof(*newinode));
	newinode->di_refcount = 1;
	return newinode;
}
