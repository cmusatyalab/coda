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

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/res/testpdlist.cc,v 4.1 97/01/08 21:50:05 rvb Exp $";
#endif /*_BLURB_*/






/* 
 * testpdlist.c 
 * test program for the portable dlist package and the memory manager 
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <struct.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include "logalloc.h"
#include "pdlist.h"

class log {
friend	log *GetLogEntry(PMemMgr*);
friend	void main(int, char**);
    int	opcode;
    char name[16];
  public:
    pdlink link;
    log();
    ~log();
    void print();
};

log::log() {
    printf("Log constructor was called \n");
}
log::~log() {
    printf("Log destructor was called\n");
}
void log::print() {
    printf("(addr = %x)op = %d; name = %s\n", this, opcode, name);
}

log *GetLogEntry(PMemMgr *memmgr) {
    log *newlog = (log *)(memmgr->NewMem());
    if (newlog){
	while(1) {
	    printf("Enter Opcode: ");
	    if (scanf("%d", &newlog->opcode) != 1) continue;
	    printf("Enter name: ");
	    if (scanf("%s", newlog->name) != 1) continue;
	    break;
	}
	newlog->link.prev = newlog->link.next = -1;
	return(newlog);
    }
    else
	return(0);
}

void main(int argc, char **argv) {
    PMemMgr	*logger;
    logger = new PMemMgr(sizeof(log), 0);
    pdlist *list = new pdlist(fldoff(log, link), logger);

    while (1) {
	log *le;
	pdlink *pdl;
	char cmd;
	printf("Enter Command: ");
	scanf("%c", &cmd);
	switch (cmd) {
	  case 'a':
	    le = GetLogEntry(logger);
	    if (le)
		list->append(&(le->link));
	    else {
		printf("Coulnt get log entry \n");
		continue;
	    }
	    break;
	  case 'i':
	    le = GetLogEntry(logger);
	    if (le)
		list->prepend(&(le->link));
	    else {
		printf("Coulnt get log entry \n");
		continue;
	    }
	    break;
	  case 'r':/* remove an entry */
	    {
		int j;
		printf("Please Enter the opcode you want to delete: ");
		scanf("%d", &j);
		pdlist_iterator	find(*list);
		while(pdl = find()){
		    le = strbase(log, pdl, link);
		    if (le->opcode == j){
			printf("Found entry \n");
			le->print();
			list->remove(pdl);
			logger->FreeMem((char *)le);
			break;
		    }
		}
	    }
	    break;
	  case 'f':
	    pdl = list->first();
	    if (pdl){
		le = strbase(log, pdl, link);
		le->print();
	    }
	    else
		printf("List is empty \n");
	    break;
	  case 'l':
	    pdl = list->last();
	    if (pdl){
		le = strbase(log, pdl, link);
		le->print();
	    }
	    else
		printf("List is empty \n");
	    break;
	  case 'p':
	    {
		printf("Printing List :\n");
		pdlist_iterator	next(*list);
		while(pdl = next()) {
		    le = strbase(log, pdl, link);
		    le->print();
		}
		printf("End of List\n");
	    }
	    break;
	  case 'q':
	    exit(0);
	  default:
	    printf("Unknown command option %c\n", cmd);
	    continue;
	}
    }
}
