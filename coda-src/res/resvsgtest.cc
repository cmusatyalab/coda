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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./coda-src/res/resvsgtest.cc,v 1.1 1996/11/22 19:13:03 braam Exp $";
#endif /*_BLURB_*/






/* test program for vsgs 
 * Created Puneet Kumar, June 1990
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <libc.h>
#include <strings.h>
#include <netdb.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "resvsg.h" 


#define VSGPATH "/tmp/VSGDB"
void main(int argc, char **argv){
    char string[1024];
    FILE *fp;
    unsigned long vsgaddr;
    struct hostent *he;
    unsigned long Haddr[VSG_MEMBERS];
    char Host[VSG_MEMBERS][256];

/*
    ResVsgInit(); 
*/
    res_vsgent::nvsgs = 0;
    res_vsgent::vsgtab = new olist;

    if ((fp = fopen(VSGPATH, "r")) == NULL){
	printf("Error while opening %s\n", VSGPATH);
	exit(-1);
    }
    while(1){
	if (fgets(string, 1024, fp) == NULL) break;
	int i = sscanf(string, "%x %s %s %s	%s %s %s %s %s\n",
		       &vsgaddr, Host[0], Host[1], Host[2], Host[3], 
		       Host[4], Host[5], Host[6], Host[7]);
	if (i == 0) break;
	/* number of hosts = i - 1 */
	for (int j = 0; j < i - 1; j++){
	    he = gethostbyname(Host[j]);
	    Haddr[j] = ntohl(*(unsigned long *)(he->h_addr));
	}
	res_vsgent *newrv = new res_vsgent(vsgaddr, Haddr, i-1);
	if (!AddMember(newrv)){
	    printf("Couldnt add member\n");
	    newrv->print();
	    delete newrv;
	}
    }
    fclose(fp);
    
    /* print different vsg groups */
    while (1) {
	int nh;
	printf("Group Number? ");
	scanf("%x", &vsgaddr);
	if (!GetHosts(vsgaddr, Haddr, &nh))
	    printf("No such address 0x%x", vsgaddr);
	else 
	    for (int i = 0; i < nh; i++)
		printf("0x%x ", Haddr[i]);
	printf("\n");
    }
    /* print the VSG group */
    res_vsg_iterator	next;
    res_vsgent *rv;
    while(rv = next())
	rv->print();
    printf("End of VSG group \n");

}
