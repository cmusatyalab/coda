#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: blurb.doc,v 1.1 96/11/22 13:29:31 raiff Exp $";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/

/*
  * This is just a test program to test out the rvmdir package
  * along with the dhash and dlist packages 
 */
#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <libc.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "dhash.h"
#include "rvmdir.h"

int FidHash(void *f)
{
    struct VFid *vf = (struct VFid *)f;
    return(vf->volume + vf->vnode + vf->vunique);
}

int FidCmp(shadowDirPage *a, shadowDirPage *b)
{
    if (a->Fid.volume < b->Fid.volume) return -1;
    if (a->Fid.volume > b->Fid.volume) return 1;
    if (a->Fid.vnode < b->Fid.vnode) return -1;
    if (a->Fid.vnode > b->Fid.vnode) return 1;
    if (a->Fid.vunique < b->Fid.vunique) return -1;
    if (a->Fid.vunique > b->Fid.vunique) return 1;
    if (a->PageNum < b->PageNum) return -1;
    if (a->PageNum > b->PageNum) return 1;
    return 0;
}


void dirtest()
{
    struct VFid vf;
    long    unique = 0;
    dhashtab *htb = new dhashtab(32, FidHash, (CFN)FidCmp);
    char    data[PAGESIZE];

    for (int i = 0; i < 3; i++){
	shadowDirPage *dirpage;
	vf.volume = i * 236;
	vf.vnode = i;
	vf.vunique = ++unique;
	dirpage = new shadowDirPage(vf, 0, data);
	printf("Inserting page....\n");
	dirpage->print();
	htb->insertUnique((void *)&(dirpage->Fid), dirpage);
	dirpage = new shadowDirPage(vf, 1, data);
	printf("Inserting page....\n");
	dirpage->print();
	htb->insertUnique((void *)&(dirpage->Fid), dirpage);
	dirpage =  new shadowDirPage(vf, 0, data);
	printf("Inserting page....\n");
	dirpage->print();
	htb->insertUnique((void *)&(dirpage->Fid), dirpage);
    }

    printf("Printing Hash Table \n");
    dhashtab_iterator	next(*htb, (void *)-1);
    shadowDirPage *page;
    while(page = (shadowDirPage *)next())
	page->print();
}
shadowDirPage::shadowDirPage(struct VFid vfid, int pagenum, char *data)
{
    Fid = vfid;
    PageNum = pagenum;
    bcopy((void *)data, (void *)&Data, PAGESIZE);
}

shadowDirPage::~shadowDirPage()
{
}

void shadowDirPage::print()
{
    print(stdout);
}
void shadowDirPage::print(FILE *fp)
{
    fflush(fp);
    print(fileno(fp));
}

void shadowDirPage::print(int fd)
{
    char buf[80];
    sprintf(buf, "Address of Dirpage:%#08x\n", (long)this);
    write(fd, buf, strlen(buf));

    sprintf(buf, "Fid = %#08x.%#08x.%#08x Page = %d\n", Fid.volume, Fid.vnode, Fid.vunique, PageNum);
    write(fd, buf, strlen(buf));
}

main()
{
    dirtest();
}

