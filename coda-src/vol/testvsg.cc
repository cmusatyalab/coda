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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/vol/Attic/testvsg.cc,v 4.1 1997/01/08 21:52:17 rvb Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#ifdef __MACH__
#include <libc.h>
#include <sysent.h>
#endif /* __MACH__ */
#ifdef __NetBSD__
#include <unistd.h>
#include <stdlib.h>
#endif __NetBSD__
#include <struct.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "vsg.h"

void PrintVSGDB() {
    ohashtab_iterator next(*vsgent::vsgaddrtab, (void *)-1);
    
    vsgent *v;
    olink *l;
    while (l = next()) {
	v = strbase(vsgent, l, vsgtabhandle);
        v->print();
    }
}
main(int argc, char **argv) {
    char input;
    InitVSGDB();
    printf("Printing VSG data base after reading it\n");
    PrintVSGDB();

    while (1) {
	char buf[80];
	printf("Option(v, h)? "); gets(buf);
	sscanf(buf, "%c", &input);
	switch (input) {
	  case 'v': {
	      long vnum;
	      long hosts[VSG_MEMBERS];
	      int nh;
	      printf("vsg address? "); gets(buf);
	      sscanf(buf, "%x", &vnum);
	      printf("The vsg number is 0x%x\n", vnum);
	      if (GetHosts(vnum, hosts, &nh)) {
		  printf("GetHosts returns %d hosts in vsg 0x%x\n", 
			 nh, vnum);
		  for (int j = 0; j < nh; j++) 
		      printf(" 0x%x", hosts[j]);
		  printf("\n");
	      }
	  }
	    break;
	  case 'h': {
	      long hosts[VSG_MEMBERS];
	      int nh;
	      char buf[80];
	      printf("number of hosts? "); gets(buf);
	      sscanf(buf, "%d", &nh);
	      printf("hosts? "); 
	      for (int j = 0; j < nh; j++) {
		  gets(buf);
		  sscanf(buf, "%x", &hosts[j]);
		  printf("hosts? ");
	      }
	      long vsgnum = GetVSGAddress(hosts, nh);
	      if (vsgnum) printf("The vsg is 0x%x\n", vsgnum);
	      else printf("vsg not found \n");
	  }
	    break;
	  default: printf("unknown option\n");
	}
    }
}
