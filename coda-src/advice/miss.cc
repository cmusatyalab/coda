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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/miss.cc,v 4.5 1998/11/30 11:39:13 jaharkes Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdlib.h>
#include "coda_assert.h" 
#include <struct.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

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

    CODA_ASSERT(current != NULL);
    existing_entry = current->get(&queue_handle);
    if (existing_entry != NULL) {
        miss *existing_miss = strbase(miss, existing_entry, queue_handle);
	CODA_ASSERT(existing_miss != NULL);
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
    CODA_ASSERT(current != NULL);
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
	CODA_ASSERT(m != NULL);
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

    CODA_ASSERT(b1 != NULL);
    CODA_ASSERT(b2 != NULL);

    m1 = strbase(miss, b1, queue_handle);
    m2 = strbase(miss, b2, queue_handle);

    CODA_ASSERT(m1 != NULL);
    CODA_ASSERT(m2 != NULL);

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
    CODA_ASSERT(previous != NULL);
    previous->clear();
    delete previous;
    previous = NULL;
}

void ReinstatePreviousMissQueue() {
    int i, num;

    CODA_ASSERT(previous != NULL);
    num = previous->count();
    for (i = 0; i < num; i++){
	bsnode *b = previous->get(BstGetMin);
	CODA_ASSERT(b != NULL);
	miss *m = strbase(miss, b, queue_handle);
	CODA_ASSERT(m != NULL);
	CODA_ASSERT(current != NULL);
	current->insert(&(m->queue_handle));
    }
    CODA_ASSERT(previous != NULL);
    num = previous->count();
    CODA_ASSERT(num == 0);

    delete previous;
    previous = NULL;
}

void OutputMissStatistics() {

    // First move current to previous and create a new current
    CODA_ASSERT(previous == NULL);
    previous = current;
    current = new bstree(PathnamePriorityFN);

    // Generate the input to the tcl script
    PrintMissList(TMPMISSLIST);

    ClearPreviousMissQueue();
}
