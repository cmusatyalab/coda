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
#include <unistd.h>
#include <stdlib.h>
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
