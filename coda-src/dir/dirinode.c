/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
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
	CODA_ASSERT(pdh);

	for (i = 0; i < DIR_MAXPAGES; i++) {
		if (pdi->di_pages[i] == 0) 
			break;
		memcpy(&pdh[i * DIR_PAGESIZE], pdi->di_pages[i], DIR_PAGESIZE);
	}
	return (PDirHeader) pdh;
}

/* allocate a new directory inode in RVM */
static PDirInode DI_New()
{
	PDirInode newinode;
	DIR_intrans();

	DLog(29, "Entering DI_New");

	newinode = (PDirInode)rvmlib_rec_malloc(sizeof(*newinode));
	CODA_ASSERT(newinode);
	rvmlib_set_range(newinode, sizeof(newinode));
	memset(newinode, 0, sizeof(*newinode));
	newinode->di_refcount = 1;
	return newinode;
}

/* copy a dir handle to a directory inode; create latter if needed */
void DI_DhToDi(PDCEntry pdce)
{
	PDirHandle pdh = DC_DC2DH(pdce);
	int pages;
	int i;
	PDirInode pdi = DC_DC2DI(pdce);

	DIR_intrans();

	CODA_ASSERT(pdh);
	pages = DH_Length(pdh)/DIR_PAGESIZE;

	if (pdi == NULL) {
		pdi = DI_New();
		DC_SetDI(pdce, pdi);
	} 

	rvmlib_set_range(pdi, sizeof(*pdi));
	pdi->di_refcount = DC_Refcount(pdce);
	
	/* copy pages to the dir inode */
	for ( i=0 ; i<pages ; i++) {
		if ( pdi->di_pages[i] == NULL ) {
			pdi->di_pages[i] = rvmlib_rec_malloc(DIR_PAGESIZE);
			CODA_ASSERT(pdi->di_pages[i]); 
		}
		rvmlib_set_range(pdi->di_pages[i], DIR_PAGESIZE);
		memcpy(pdi->di_pages[i],DIR_Page(pdh->dh_data,i), DIR_PAGESIZE);
	}

	/* free pages which have disappeared */
	for (i=pages ; i<DIR_MAXPAGES ; i++) {
		if (pdi->di_pages[i]) {
			rvmlib_rec_free(pdi->di_pages[i]);
                        pdi->di_pages[i] = NULL;
                }
	}

	return;
}

/* reduce the refcount of the directory, delete it when it falls to 0 */
void DI_Dec(PDirInode pdi)
{
	int rcount;
	int i;

	DIR_intrans();

	CODA_ASSERT(pdi); 
	rcount = pdi->di_refcount;
	if (rcount == 1){
		/* Last vnode referencing directory inode - delete it */
		for (i = 0; i < DIR_MAXPAGES; i++) {
			if (pdi->di_pages[i]){
				DLog(29, "Deleting page %d for directory", i);
				rvmlib_rec_free(pdi->di_pages[i]);
                                // shouldn't matter pdi is gone anyways.
                                //pdi->di_pages[i] = NULL;
                        }
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

        CODA_ASSERT(pdi);
	rcount = pdi->di_refcount;
	rcount++;
	RVMLIB_MODIFY(pdi->di_refcount, rcount);
}

/* return the refcoount of a directory */
int DI_Count(PDirInode pdi)
{
        CODA_ASSERT(pdi);
	return  pdi->di_refcount;
}

/* return the number of pages in a directory */
int DI_Pages(PDirInode pdi)
{
	int i = 0;
	CODA_ASSERT(pdi);

	while(i < DIR_MAXPAGES && pdi->di_pages[i]) 
		i++;
	
	return i;
}

/* return a pointer to a page in a directory */
void *DI_Page(PDirInode pdi, int page)
{
	
	CODA_ASSERT(pdi);
	CODA_ASSERT(page>=0 && page < DIR_MAXPAGES);

	return pdi->di_pages[page];
}


/* copies oldinode and its pages by first allocating a newinode */
void DI_Copy(PDirInode oldinode, PDirInode *newinode)
{
	int i;

	DIR_intrans();

	DLog(29, "Entering DI_Copy(%p , %p)", oldinode, newinode);
	CODA_ASSERT(oldinode);

	*newinode = (PDirInode)rvmlib_rec_malloc(sizeof(**newinode));
	CODA_ASSERT(*newinode);
	rvmlib_set_range(*newinode, sizeof(*newinode));

	memset(*newinode, 0, sizeof(**newinode));
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
	CODA_ASSERT(oldinode);

	*newinode = (PDirInode)malloc(sizeof(**newinode));
	CODA_ASSERT(*newinode);

	memset(*newinode, 0, sizeof(**newinode));
	for(i = 0; i < DIR_MAXPAGES; i++)
		if (oldinode->di_pages[i]){
			DLog(29, "CopyDirInode: Copying page %d", i);
			(*newinode)->di_pages[i] = malloc(DIR_PAGESIZE);
			memcpy(oldinode->di_pages[i], (*newinode)->di_pages[i], 
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

	CODA_ASSERT(pdi); 
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
	CODA_ASSERT(pdi); 
	pdi->di_refcount = 1;
	DI_VMDec(pdi);
}
