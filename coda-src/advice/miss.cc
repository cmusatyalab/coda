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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/miss.cc,v 4.2 1997/02/26 16:02:28 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdlib.h>
#include <assert.h> 
#include <struct.h>
#include <strings.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include "mybstree.h"
#include "advice_srv.h"
#include "miss.h"


/* Data structures for holding the ordered list of disconnected cache misses */
bstree *current;            /* This version is the one in which to insert new elements */
bstree *previous;           /* This version is presently being processed by the user */



/**************
 * CLASS MISS *
 **************/

miss::miss(char *Path, char *Program) {
    bsnode *existing_entry; 
    
    path = new char[strlen(Path)+1];
    program = new char[strlen(Program)+1];

    LogMsg(0,LogLevel,LogFile, "E miss(%s,%s)", Path, Program);

    strcpy(path, Path);
    strcpy(program, Program);

    assert(current != NULL);
    existing_entry = current->get(&queue_handle);
    if (existing_entry != NULL) {
        miss *existing_miss = strbase(miss, existing_entry, queue_handle);
	assert(existing_miss != NULL);
	existing_miss->num_instances++;
	delete this;
    } else {
        num_instances = 1;
	current->insert(&queue_handle);
    }
}

miss::miss(miss& m) {
    abort();
}

miss::operator=(miss& m) {
    abort();
    return(0);
}

miss::~miss() {
    assert(current != NULL);
    current->remove(&queue_handle);
    delete[] path;
    delete[] program;
}

void miss::print(FILE *outfile) {
    fprintf(outfile, "%s & %s (%d)\n", path, program, num_instances);
}

// Friend of CLASS MISS
void PrintMissList(char *filename) {
    FILE *outfile;
    bsnode *b;
    bstree_iterator next(*previous, BstDescending);
 
    outfile = fopen(filename, "w+");
    if (outfile == NULL) {
	LogMsg(0,LogLevel,LogFile,"Failed to open %s for printing the miss list",filename);
	return;
    }

    while (b = next()) {
        miss *m = strbase(miss, b, queue_handle);
	assert(m != NULL);
	m->print(outfile);
    }

    fflush(outfile);
    fclose(outfile);
}

/* 
 * Friend of CLASS MISS
 * 
 * Compare first based upon object's priority, then alphabetically by pathname.
 */
int PathnamePriorityFN(bsnode *b1, bsnode *b2) {
    int pathcmp;
    int programcmp;
    int rc;
    miss *m1; 
    miss *m2;

    assert(b1 != NULL);
    assert(b2 != NULL);

    m1 = strbase(miss, b1, queue_handle);
    m2 = strbase(miss, b2, queue_handle);

    assert(m1 != NULL);
    assert(m2 != NULL);

    pathcmp = strcmp(m1->path, m2->path);
    programcmp = strcmp(m1->program, m2->program);
    rc = pathcmp?pathcmp:programcmp;
    return(rc);
}

/**************************************
 * Helper routines for the advice_srv *
 **************************************/

void InitMissQueue() {
    current = new bstree(PathnamePriorityFN);
    previous = NULL;
}

void ClearPreviousMissQueue() {
    assert(previous != NULL);
    previous->clear();
    delete previous;
    previous = NULL;
}

void ReinstatePreviousMissQueue() {
    int i, num;

    assert(previous != NULL);
    num = previous->count();
    for (i = 0; i < num; i++){
	bsnode *b = previous->get(BstGetMin);
	assert(b != NULL);
	miss *m = strbase(miss, b, queue_handle);
	assert(m != NULL);
	assert(current != NULL);
	current->insert(&(m->queue_handle));
    }
    assert(previous != NULL);
    num = previous->count();
    assert(num == 0);

    delete previous;
    previous = NULL;
}

void HandleWeakAdvice() {
    struct stat buf;
    char command[256];
    int rc;

    // First move current to previous and create a new current
    assert(previous == NULL);
    previous = current;
    current = new bstree(PathnamePriorityFN);

    // Generate the input to the tcl script
    PrintMissList(TMPMISSLIST);

    // Execute the tcl script
    {
       char *args[3];

       args[0] = MISSLIST;
       args[1] = TMPMISSLIST;
       args[2] = NULL;

       int rc = execute_tcl(MISSLIST, args);
       if (rc == -1) {
         LogMsg(0,LogLevel,LogFile, "HandleWeakAdvice: execute_tcl ERROR");
	 ReinstatePreviousMissQueue();
       } 
       else
	 ClearPreviousMissQueue();
    }
}

