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


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <struct.h>


#ifdef __cplusplus
}
#endif __cplusplus


#include <util.h>
#include "admon.h"
#include "mybstree.h"
#include "advice_srv.h"
#include "volent.h"


bstree *VDB;



/********************
 * CLASS volent *
 ********************/

volent::volent(char *Name, VolumeId id, VolumeStates theState) {
    bsnode *existing_volume; 
    name = new char[strlen(Name)+1];

    LogMsg(100,LogLevel,LogFile, "E volent(%s,%x)", Name, id);

    strcpy(name, Name);
    vid = id;

    CODA_ASSERT(VDB != NULL);
    existing_volume = VDB->get(&queue_handle);
    if (existing_volume != NULL) {
        volent *existing_vol = strbase(volent, existing_volume, queue_handle);
	CODA_ASSERT(existing_vol != NULL);
        existing_vol->state = theState;
        delete this;
	LogMsg(100,LogLevel,LogFile, "Updating %s", Name);
    } else {
        state = theState;
	CODA_ASSERT(VDB != NULL);
        VDB->insert(&queue_handle);
	LogMsg(100,LogLevel,LogFile, "Inserting %s", Name);
    }

}

volent::volent(volent& i) {
    abort();
}

int volent::operator=(volent& i) {
    abort();
    return(0);
}

volent::~volent() {
    CODA_ASSERT(VDB != NULL);
    VDB->remove(&queue_handle);
    delete[] name;
}

char *volent::VolumeStateString() {
  static char msgbuf[32];

  switch (state) {
	case VSemulating:
	    (void) snprintf(msgbuf, 32, "Emulating");
	    break;
	case VShoarding:
	    (void) snprintf(msgbuf, 32, "Hoarding");
	    break;
	case VSlogging:
	    (void) snprintf(msgbuf, 32, "Logging");
	    break;
	case VSresolving:
	    (void) snprintf(msgbuf, 32, "Resolving");
	    break;
	default:
	    (void) snprintf(msgbuf, 32, "Unknown");
	    break;
  }

  return(msgbuf);
}

void volent::print(FILE *outfile) {
    fprintf(outfile, "%s <%x>: %s\n", name, vid, VolumeStateString());
}


// Friend of CLASS VOLENT
void PrintVDB(char *filename) {
    FILE *outfile;
    bsnode *b;
    bstree_iterator next(*VDB, BstDescending);
 
    outfile = fopen(filename, "a+");
    if (outfile == NULL) {
        LogMsg(0,LogLevel,LogFile,"Failed to open %s for printing the VDB",filename);
        return;
    }

    fprintf(outfile, "Printing VDB elements:\n");
    while (b = next()) {
        volent *vol;
        CODA_ASSERT(b != NULL);
        vol = strbase(volent, b, queue_handle);
	CODA_ASSERT(vol != NULL);
        vol->print(outfile);
    }

    fprintf(outfile, "Done.\n\n");
    fflush(outfile);
    fclose(outfile);
}


/* 
 * Friend of CLASS VOLENT
 * 
 * Compare based upon volumes id number.
 */
int VolentPriorityFN(bsnode *b1, bsnode *b2) {
    volent *v1;
    volent *v2;

    CODA_ASSERT(b1 != NULL);
    CODA_ASSERT(b2 != NULL);

    v1 = strbase(volent, b1, queue_handle);
    v2 = strbase(volent, b2, queue_handle);

    CODA_ASSERT(v1 != NULL);
    CODA_ASSERT(v2 != NULL);

    if (v1->vid == v2->vid)
	return(0);
    else if (v1->vid > v2->vid)
	return(1);
    else
	return(-1);
}

/* 
 * Friend of CLASS VOLENT
 * 
 * Determine stoplight state.
 */
StoplightStates StoplightState() {
    bsnode *b;
    bstree_iterator next(*VDB, BstDescending);
    int at_least_one_logging = 0;
    int at_least_one_disco = 0;
    int at_least_one_resolving = 0;

    if (LogLevel >= 100) 
        PrintVDB(VDBFileName);

    while (b = next()) {
        volent *vol;

	CODA_ASSERT(b != NULL);
	vol = strbase(volent, b, queue_handle);

        CODA_ASSERT(vol != NULL);

	if (vol->state == VSemulating)
	    at_least_one_disco++;

	if (vol->state == VSlogging)
	    at_least_one_logging++;

	if (vol->state == VSresolving)
	    at_least_one_resolving++;
    }

    /* Determine the state */
    if (at_least_one_disco)
	return(SLdisconnect);
    else if (at_least_one_logging)
	return(SLweak);
    else
	return(SLstrong);
}

/**************************************
 * Helper routines for the advice_srv *
 **************************************/

void InitVDB() {
    VDB = new bstree(VolentPriorityFN);
}
