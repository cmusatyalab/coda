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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/advice/Attic/volent.h,v 4.1 1997/01/08 21:49:20 rvb Exp $";
#endif /*_BLURB_*/





enum VolumeStates { VSunknown, VSemulating, VShoarding, VSlogging, VSresolving };

class volent {
  friend int VolentPriorityFN(bsnode *, bsnode *);
  friend void PrintVDB(char *);
  friend StoplightStates StoplightState();

  char *name;
  VolumeId vid;
  VolumeStates state;

  bsnode queue_handle;		/* link for the volume queue */

 public:
    volent(char *Name, VolumeId id, VolumeStates theState);
    volent(volent&);
    operator=(volent&);
    ~volent();
    setState(VolumeStates newState);
    char *VolumeStateString();
    void print(FILE *f);
};
