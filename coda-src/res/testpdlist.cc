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
