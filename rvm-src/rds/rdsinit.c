#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/rvm-src/rds/rdsinit.c,v 4.3 1997/04/01 01:57:17 clement Exp $";
#endif _BLURB_


/*
 * This module holds the routines to initialize the datadevice of a heap.
 * Hopefully it'll someday be merged with something hank wrote to initialize
 * the log.
 */

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include <rvm.h>
#include <rvm_segment.h>
#include <rvm_segment_private.h>
#include <rds.h>

#ifdef __STDC__
#include <string.h>
#define BZERO(D,L)   memset((D),0,(L))
#else
#define BZERO(D,L)   bzero((D),(L))
#endif

void
PollAndYield() {
}

extern int          errno;              /* kernel error number */

enum round_dir { UP, DOWN, NO_ROUND };

jmp_buf jmpbuf_quit;

char *usage[]={
"Usage: rdsinit [-f] log data_seg [ datalen saddr hlen slen nl chunk ] \n",
"  where\n",
"    -f       supplied parameter are firm, do not ask for confirm\n",
"    log      is the name of log file/device\n",
"    data_seg is the name of data segment file/device\n",
"    datalen  gives the length of data segment\n",
"    saddr    is the starting address of rvm\n",
"    hlen     is the heap length\n",
"    slen     is the static length\n",
"    nl       is the number of free list used by RDS\n",
"    chunk    is the chunksize\n",
""
};

PRIVATE char *welcome[]={
"\n",
"============================================================\n",
"  Getting parameters for RDS\n",
"============================================================\n",
""
};

char *explain_datalen[]={
"1) length of data segment file/device\n",
"   There will be two regions in the segment:\n",
"   one is for the heap, the other is for the static.\n",
"   Some extra space (usually one page) will also \n", 
"   be needed for the segment header.\n",
"   Length of data segment must be a multiple of pagesize.\n",
"   Please make sure you have enough space in your disk.\n",
""
};

char *explain_saddr[]={
"2) starting address of rvm\n",
"   This is where heap and static will be mapped into virtual memory, it\n",
"   must be larger than the largest possible break point of your\n",
"   application, and it should not be in conflict other use of vm (such as\n",
"   shared libraries).  Also, it must be on a page boundary\n", 
"   (In CMU, we use 0x20000000 (536870912) with Linux and BSD44,\n",
"   0x70000000 (1879048192) with Mach and BSD44 without problem.)\n",
""
};

char *explain_hlen[]={
"3) heap length\n",
"   It is the size of the dynamic heap to be managed by RDS,\n",
"   it must be an integral multiple of pagesize.\n",
""
};

char *explain_slen[]={
"4) static length\n",
"   It is the size of the statically allocated location to be\n",
"   managed by your application.\n",
"   It must be an integral multiple of pagesize.\n",
""
};

char *explain_nl[]={
"5) nlists\n",
"   It is the number of free list used by RDS.\n",
"   (nlist=100 is a reasonable choice.)\n",
""
};

char *explain_chunk[]={
"6) chunksize\n",
"   The free list are maintained in sizes of one chunk to n chunk,\n",
"   where n is the number of free lists.\n",
"   It must be an integral multiple of sizeof(char *) of your system,\n",
"   and must at least be RDS_MIN_CHUNK_SIZE.\n", 
"   (chunksize=32 is a reasonable choice.)\n",
""
};

PRIVATE void print_msg(msg)
     char **msg;
{
    int i=0;
    while (msg[i][0] != '\0')
	printf("%s", msg[i++]);
}

PRIVATE int round_to_multiple(val, n, dir)
     long           val;
     long           n;
     enum round_dir dir;
{
    long rem;

    if (dir==NO_ROUND || n==0)		/* don't round */
	return(val);
    
    if ((rem = val % n) != 0) { /* need round up/down */
	if (dir==UP)
	    return ( val + n -rem );
	else if (dir==DOWN)
	    return ( val - rem );
    } else
	return( val );

}


PRIVATE int confirm_rounded_value(pvalue, base, unit, round_dir, min)
     unsigned long  *pvalue;
     int            base;
     int            unit;
     enum round_dir round_dir;
     int            min;
{
    char          string[120];
    unsigned long t1=0, t2=0;	/* temporary values */
    
    if (base == 16) {
	snprintf(string, 120, "0x%x (in decimal %u)", *pvalue, *pvalue);
    } else if (base == 10) {
	snprintf(string, 120, "%d (in hex 0x%x)", *pvalue, *pvalue);
    } else if (base == 8) {
	snprintf(string, 120, "0%o (in decimal %u)", *pvalue, *pvalue);
    } else {
	assert(0);		/* illegal base value */
    }

    printf("   ====> You have entered %s. ", string);

    if (*pvalue<min) {
	printf("\n");
	printf("         Umm... minimum is %d (in hex 0x%x) ",
	       min, min);
        t1 = min;
    } else {
	t1 = *pvalue;
    }
    
    t2 = round_to_multiple(t1, unit, round_dir);

    if (t2 != t1) {
    	if (base == 16) {
    	    snprintf(string, 120, "0x%x (in decimal %u)", t2, t2);
    	} else if (base == 10) {
    	    snprintf(string, 120, "%d (in hex 0x%x)", t2, t2);
    	} else if (base == 8) {
    	    snprintf(string, 120, "0%o (in decimal %u)", t2, t2);
    	} else {
    	    assert(0);		/* illegal base value */
    	}

	printf("\n");
        printf("         Umm... not a multiple of %d.  I need to round it\n",
	       unit);
        printf("         %s to %s. ",
               (round_dir == UP ? "up" : "down"), string);

    }
    printf("Accept ? (y|n|q) ");
    fgets(string, 120, stdin);
    if (strcmp(string,"y\n") == 0 || strcmp(string,"Y\n") == 0
	  || strcmp(string, "\n") == 0 ) {
	*pvalue = t2;
	return 1;		/* confirmed */
    } else if (strcmp(string,"q\n")==0 || strcmp(string, "Q\n")==0)
	longjmp(jmpbuf_quit, 1); /* quit */
    else			/* all other cases: re-enter */
	return 0;		/* not confirmed */
}

/* determine if the numeric string is in hex, octal or decimal notation */
PRIVATE int find_base(string)
     char *string;
{
    if (string[0] == '0')
	return ( string[1]=='x' ? 16 : 8 );
    else
	return 10;
}
		

PRIVATE int get_valid_parm(argc, argv, pdatalen,
			   pstatic_addr, phlen, pslen,
			   pnlists,pchunksize,firm)
     int          argc;
     char         **argv;
     long         *pdatalen;
     char         **pstatic_addr;
     unsigned int *phlen;
     unsigned int *pslen;
     unsigned int *pnlists;
     unsigned int *pchunksize;
     int          firm;
{
    char string[80];
    int  paraOK=0;
    int  para_given=0;
    int  base;

    if ( argc == 9 ) {
	/* accept the 6 parameter as cmd-line argument */
	*pdatalen       = strtoul(argv[3], NULL, 0);
	*pstatic_addr = (char *)strtoul(argv[4], NULL, 0);
	*phlen        = strtoul(argv[5], NULL, 0);
	*pslen        = strtoul(argv[6], NULL, 0);
	*pnlists      = strtoul(argv[7], NULL, 0);
	*pchunksize   = strtoul(argv[8], NULL, 0);
	    
	para_given = 1;
	if (firm)
	    return;		/* no need to get confirm from user */
    }

    /* looping to get parameters from user interactively, and confirm */
    do {
	if (para_given) {
	    para_given = 0;	/* skip the first time of asking */
	    goto confirm;
	}
	print_msg(welcome);
	print_msg(explain_datalen);
        do {
            printf("   ==> please enter length of data segment:");
    	    fgets(string,80,stdin);
    	    base = find_base(string);
    	    *pdatalen = strtoul(string, NULL, 0);
	} while (!confirm_rounded_value(pdatalen, base, RVM_PAGE_SIZE, UP,
					3*RVM_PAGE_SIZE));

	print_msg(explain_saddr);
        do {
            printf("   ==> please enter starting address of rvm: ");
	    fgets(string,80,stdin);
	    base = find_base(string);
	    *pstatic_addr = (char *)strtoul(string, NULL, 0);
        } while (!confirm_rounded_value(pstatic_addr, base, RVM_PAGE_SIZE,UP,
					0x4000000)); 
				/* note 0x4000000 is just a very loose lower
				*  bound.  Actual number should be much
				*  larger (and system-dependent)
				*/
    get_heap_n_static:
	print_msg(explain_hlen);
        do {
            printf("   ==> please enter the heap length: ");
	    fgets(string,80,stdin);
	    base = find_base(string);
	    *phlen = strtoul(string, NULL, 0);
        } while (!confirm_rounded_value(phlen, base, RVM_PAGE_SIZE, UP,
					RVM_PAGE_SIZE));

        print_msg(explain_slen);
        do {
            printf("   ==> please enter the static length: ");
	    fgets(string,80,stdin);
            base = find_base(string);
	    *pslen = strtoul(string, NULL, 0);
	} while (!confirm_rounded_value(pslen, base, RVM_PAGE_SIZE, UP,
					RVM_PAGE_SIZE));

	if (*pdatalen<*phlen+*pslen+RVM_SEGMENT_HDR_SIZE) {
	    printf("\n");
	    printf("   Sorry ! your heap len + static len is too large !\n");
	    printf("   their sum must be less than 0x%x (%d)\n", 
		   *pdatalen-RVM_PAGE_SIZE, *pdatalen-RVM_SEGMENT_HDR_SIZE);
	    printf("   please re-enter\n\n");
	    goto get_heap_n_static;
	}

	print_msg(explain_nl);
        do {
            printf("   ==> please enter a decimal value for nlists: ");
	    fgets(string,80,stdin);
	    base = find_base(string);
	    *pnlists = strtoul(string, NULL, 0);
        } while (!confirm_rounded_value(pnlists, base, 0, NO_ROUND,
					1));

	print_msg(explain_chunk);
        do {
            printf("   ==> please enter a decimal value for chunksize: ");
	    fgets(string,80,stdin);
	    base = find_base(string);
	    *pchunksize = strtoul(string, NULL, 0);
	} while (!confirm_rounded_value(pchunksize, base, sizeof(char *), UP,
					32));

    confirm:
	printf("\n");
        printf("The following parameters are chosen:\n");
        printf("   length of data segment: %#10x (%10d)\n",
	       *pdatalen, *pdatalen);
        printf("  starting address of rvm: %#10x (%10d)\n",
	       *pstatic_addr, *pstatic_addr);
        printf("                 heap len: %#10x (%10d)\n",
	       *phlen, *phlen);
        printf("               static len: %#10x (%10d)\n",
	       *pslen, *pslen);
        printf("                   nlists: %#10x (%10d)\n",
	       *pnlists, *pnlists);
        printf("               chunk size: %#10x (%10d)\n",
	       *pchunksize, *pchunksize);
        printf("Do you agree with these parameters ? (y|n|q) ");
	fgets(string,80,stdin);
	if (strcmp(string,"y\n") == 0 || strcmp(string,"Y\n") == 0
	    || strcmp(string, "\n") == 0 )
	    paraOK = 1;
	else if (strcmp(string,"q\n") == 0 || strcmp(string,"Q\n") == 0)
	    longjmp(jmpbuf_quit,1);
    } while (!paraOK);
    return;
}



extern char *ortarg;
extern int optind, opterr, optopt;


int main(argc, argv)
     int  argc;
     char *argv[];
{
    rvm_options_t       *options;       /* options descriptor ptr */
    rvm_return_t	ret;
    int err, fd, i;
    char string[80], *static_addr=NULL, *sptr, buf[4096];
    char *logName, *dataName;
    unsigned int slen=0, hlen=0, nlists=0, chunksize=0;
    rvm_offset_t DataLen;
    long         datalen=0;
    int firm=0, opt_unknown=0;
    int c, arg_used=0;

    /* first check if we have -l and -c options */
    while ( (c = getopt(argc, argv, "f")) != EOF ) {	
	switch (c) {
	case 'f':
	    firm = 1;
	    arg_used ++;
	    break;
	default:
	    opt_unknown = 1;
	    break;
	}
    }
    argc -= arg_used;
    argv += arg_used;
	    
    if ((argc !=3 && argc !=9) || opt_unknown) {
	print_msg(usage);
	exit(-1);
    }

    logName = argv[1];
    dataName = argv[2];

    /* initialized RVM */
    options = rvm_malloc_options();
    options->log_dev = logName;
	      
    ret = RVM_INIT(options);
    if  (ret != RVM_SUCCESS) {
	printf("?  rvm_initialize failed, code: %s\n",rvm_return(ret));
	exit(-1);
    } else
	printf("rvm_initialize succeeded.\n");

    if (setjmp(jmpbuf_quit) != 0) {
        printf("rdsinit quit.  No permanent change is made\n");
        exit(-1);
    }

    ret = get_valid_parm(argc, argv, &datalen,
			 &static_addr, &hlen, &slen, &nlists, &chunksize,
			 firm);
    if (ret < 0) {
	printf("? invalid rdsinit parameters\n");
        printf("rdsinit quit.  No permanent change is made\n");
	exit(-1);
    }

    fd = open(dataName, O_WRONLY | O_CREAT | O_TRUNC, 00644);
    if (fd < 0) {
	printf("?  Couldn't truncate %s.\n", dataName);
	exit(-1);
    }

    printf("Going to initialize data file to zero, could take awhile.\n");
    lseek(fd, 0, 0);
    BZERO(buf, 4096);
    for (i = 0; i < datalen; i+= 4096) {
	if (write(fd, buf, 4096) != 4096) {
	    printf("?  Couldn't write to %s.\n", dataName);
	    exit(-1);
	}
    }
    printf("done.\n");
    
    close(fd);


    DataLen = RVM_MK_OFFSET(0, datalen);
    rds_zap_heap(dataName, DataLen, static_addr, slen, hlen, nlists,
		 chunksize, &err);
    if (err == SUCCESS)
	printf("rds_zap_heap completed successfully.\n");
    else
        {
        if (err > SUCCESS)
            printf("?  ERROR: rds_zap_heap %s.\n",
                   rvm_return(err));
        else
            printf("?  ERROR: rds_zap_heap, code: %d\n", err);
        exit(-1);
        }

    ret = rvm_terminate();
    if (ret != RVM_SUCCESS)
        {
	printf("\n? Error in rvm_terminate, ret = %s\n", rvm_return(ret));
        exit(-1);
        }
    else
	printf("rvm_terminate succeeded.\n");

    return 0;
}


























