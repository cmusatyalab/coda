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






/* test program for vsgs 
 * Created Puneet Kumar, June 1990
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <netdb.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "resvsg.h" 


void main(int argc, char **argv)
{
    char string[1024];
    FILE *fp;
    unsigned long vsgaddr;
    struct hostent *he;
    unsigned long Haddr[VSG_MEMBERS];
    char Host[VSG_MEMBERS][256];


    if ( argc != 2 ) {
	    printf("Usage %s VSGDB\n", argv[0]);
	    exit(1);
    }

    res_vsgent::nvsgs = 0;
    res_vsgent::vsgtab = new olist;

    if ((fp = fopen(argv[1], "r")) == NULL) {
	printf("Error while opening %s\n", argv[1]);
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
	    if ( he == NULL ) {
		    herror(string);
		    exit(1);
	    }
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
