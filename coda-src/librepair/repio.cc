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








/* Created:
	M. Satyanarayanan
	June 1989

    repair_putdfile() and repair_getdfile()
	Routines to transfer directory repair info to/from memory and file
	Integer data is stored in network order in file.
	File format is as follows:
		unsigned int replicaCount;
		unsigned int replicaId[0];
		unsigned int repairCount[0];
		unsigned int replicaId[1];
		unsigned int repairCount[1];
		......................
		unsigned int replicaId[replicaCount-1];
		unsigned int repairCount[replicaCount-1];
		struct repair repairList[0..repairCount[0]];
		struct repair repairList[0..repairCount[1]];
		........................................
		struct repair repairList[0..repairCount[replicaCount-1]];
	Strings are terminated with '\n' (not '\0') to satisfy fputs()/fgets()

	In-core format is as follows:
		unsigned int replicaCount;
	        struct listhdr replicaList[replicaCount];
		(Each struct listhdr contains a pointer to its struct repair array.)
	
    repair_parseline()
	Routine to parse an ASCII line and extract a repair list entry from it.
	
    repair_parsefile()
	Routine to parse an ASCII file and extract entire set of repair lists from it.
	
    repair_printline()
	Routine to print out a repair list entry

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <errno.h>
#include "coda_assert.h"
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdlib.h>
#include <prs_fs.h>
#include <rpc2.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include "repio.h"

static char *eatwhite(char *), *eatnonwhite(char *);
static int acldecode(char *, unsigned int *);
static int growarray(char **arrayaddr, int *arraysize, int elemsize);


int repair_putdfile(char *fname, int replicaCount, struct listhdr *replicaList)
    /*
	fname		name of file to write list to
	replicaCount 	no of replicas
	replicaList	array of headers, one per replica

	Returns 0 on success.
	Returns -1 on failure, with msg on stderr.
    */
    {
    register int i, j, k;
    register unsigned int x;
    struct repair *r;
    FILE *ff;
    
    ff = fopen(fname,"w");
    if (!ff) {perror(fname); return(-1);}
    
    /* Write out number of replicas */
    x = htonl(replicaCount);
    putw((int)x, ff);
    
    /* Write out header for each replica */
    for (i = 0; i < replicaCount; i++)
    	{
	x = htonl(replicaList[i].replicaId);
	putw(x, ff);
	x = htonl(replicaList[i].repairCount);
	putw(x, ff);
	}

    /* Write out list of repairs for each replica */
    for (i = 0; i < replicaCount; i++)
    	{
	r = replicaList[i].repairList;
	for (j = 0; j < replicaList[i].repairCount; j++)
	    {
	    x = htonl(r[j].opcode);
	    putw(x, ff);
	    fputs(r[j].name, ff);
	    fputs("\n", ff);
	    fputs(r[j].newname, ff);
	    fputs("\n", ff);	    
	    for (k = 0; k < REPAIR_MAX; k++)
		{
		x = htonl(r[j].parms[k]);
		putw(x, ff);
		}
	    }
	}

    /* Do a batch check for I/O errors; we don't worry about
       checking each I/O opn, since the only expected error is diskfull and
       this is likely to persist till the end */
    if (ferror(ff)) {perror(fname); fclose(ff); return(-1);}
    else {fclose(ff); return(0);}
    }


int repair_getdfile(char *fname, int *replicaCount, struct listhdr **replicaList)
    /*
	fname		name of file to read list from
	replicaCount	number of replicas
	replicaList		array of malloc'ed headers, one per replica

	Returns 0 on success, after malloc'ing and filling replicaList and replicaCount
	Returns -1 on failure, after printing msg on stderr.
	
	When done, caller should  release storage malloc'ed here.
    */

    {
    register int i, j, k;
    register unsigned int x;
    struct repair *r;
    FILE *ff;
    char *s;
    
#define CHKERR() if (ferror(ff) || feof(ff)) goto ERR

    ff = fopen(fname, "r");
    if (!ff) goto ERR;
    
    x = getw(ff); CHKERR();
    *replicaCount = ntohl(x);

    *replicaList = (struct listhdr *) calloc(*replicaCount, sizeof(struct listhdr));

    for (i = 0; i < *replicaCount; i++)
	{
	x = getw(ff); CHKERR();
	(*replicaList)[i].replicaId  = ntohl(x);
	x = getw(ff); CHKERR();
	(*replicaList)[i].repairCount  = ntohl(x);	
	}

    for (i = 0; i < *replicaCount; i++)
    	{
	if ((*replicaList)[i].repairCount > 0){
	    r = (struct repair *) calloc((*replicaList)[i].repairCount,
					 sizeof(struct repair));
	    if (!r)  return(-1);
	    (*replicaList)[i].repairList = r;
	}
	else {
	    (*replicaList)[i].repairList = NULL;
	    continue;
	}
	
	for (j = 0; j < (*replicaList)[i].repairCount; j++)
	{
	    x = getw(ff); CHKERR();
	    r[j].opcode = ntohl(x);

	    s = r[j].name;
	    fgets(s, MAXNAMELEN, ff); CHKERR();
	    *(s + strlen(s) - 1) = 0;  /* nuke the '\n' left behind by fgets() */

	    s = r[j].newname;
	    fgets(s, MAXNAMELEN, ff); CHKERR();
	    *(s + strlen(s) - 1) = 0;  /* nuke the '\n' left behind by fgets() */

	    for (k = 0; k < REPAIR_MAX; k++)
	    {
		x = getw(ff); CHKERR();
		r[j].parms[k] = ntohl(x);
	    }
	}
    }
    fclose(ff);
    return(0);
    
    /* Error exit */
ERR: perror(fname); fclose(ff); return(-1);            
    }



int repair_parseline(char *line, struct repair *rs)

    /*  Parses line and fills rs
        Returns 0 on success, -1 on syntax error, -2 on blankline
	
	Note: line gets clobbered during parsing
	
        Each line is of the form
	    "<wsp><opcode><wsp><name>[<wsp><p1><wsp><p2>.....]"
	where
	    <opcode> and <name> are strings, and <p1>, <p2> etc are in hex
	    and <wsp> is whitespace (blanks and tabs)
	For the opcode setacl, p1 should specify rights as [rliwdka]	
	Some opcodes don't use a name field
    */
    {
    register char *c, *d, *eos;
    register int i;


#define NEXTFIELD()\
    /* Set c to start of next field, d to the null at the end of this field */\
    c = eatwhite(c);    /* consume leading whitespace */\
    if (!*c) return(-1); /* premature eof */\
    d = eatnonwhite(c); /* consume field and stop at wsp char */\
    *d = 0;             /* insert string terminator */
    
#define ADVANCE()\
    /* Advance both c and d beyond the current field */\
    c = ++d;\
    if (eos - c <= 0) return(-1);  /* premature eof */
    
    
    bzero((void *)rs, sizeof(struct repair));  /* init all fields to 0 */
    c = line; /* start at the beginning */
    eos = line + strlen(line); /* note the end */

    /* Blank line? */
    c = eatwhite(c); /* swallow leading whitespace */
    if (!*c) return(-2); /* only place premature eof is ok */

    /* Parse opcode */    
    NEXTFIELD();
    if (!strcmp(c, "createf")) {rs->opcode = REPAIR_CREATEF; goto Opfound;}
    if (!strcmp(c, "created")) {rs->opcode = REPAIR_CREATED; goto Opfound;}
    if (!strcmp(c, "creates")) {rs->opcode = REPAIR_CREATES; goto Opfound;}
    if (!strcmp(c, "createl")) {rs->opcode = REPAIR_CREATEL; goto Opfound;}
    if (!strcmp(c, "removefsl"))  {rs->opcode = REPAIR_REMOVEFSL;  goto Opfound;}
    if (!strcmp(c, "removed"))  {rs->opcode = REPAIR_REMOVED;  goto Opfound;}
    if (!strcmp(c, "setacl"))  {rs->opcode = REPAIR_SETACL;  goto Opfound;}
    if (!strcmp(c, "setnacl"))  {rs->opcode = REPAIR_SETNACL;  goto Opfound;}
    if (!strcmp(c, "setmode"))   {rs->opcode = REPAIR_SETMODE;   goto Opfound;}
    if (!strcmp(c, "setowner"))  {rs->opcode = REPAIR_SETOWNER;  goto Opfound;}
    if (!strcmp(c, "setmtime"))  {rs->opcode = REPAIR_SETMTIME;  goto Opfound;}
    if (!strcmp(c, "replica"))  {rs->opcode = REPAIR_REPLICA;  goto Opfound;}
    if (!strcmp(c, "mv"))  {rs->opcode = REPAIR_RENAME;  goto Opfound;}
    
    return(-1);  /* unknown opcode */
Opfound:

    /* Parse name */
    switch(rs->opcode)
	{
	case REPAIR_SETMODE:
	case REPAIR_SETOWNER:
	case REPAIR_SETMTIME:
	    break;      /* These opcodes doesn't use a name */
	    
	case REPAIR_RENAME:
	    ADVANCE();
	    NEXTFIELD();
	    if (strlen(c) >= MAXNAMELEN) return(-1);
	    else strcpy(rs->name, c);

	    /* get the new name too */
	    ADVANCE();
	    NEXTFIELD();
	    if (strlen(c) >= MAXNAMELEN) return(-1);
	    else strcpy(rs->newname, c);
	    break;
	    
	default:
	    ADVANCE();
	    NEXTFIELD();
	    if (strlen(c) >= MAXNAMELEN) return(-1);
	    else strcpy(rs->name, c);
	    break;
	}

    /* Parse parms */
    for (i = 0; i < REPAIR_MAX; i++)
	{
	/* Quit looking if enough parms found */
	switch(rs->opcode)
	    {
	    case REPAIR_REMOVEFSL:
	    case REPAIR_REMOVED:
		if (i >= 0) goto DoneParse; /* "else" can never happen */
		
	    case REPAIR_REPLICA:
	    case REPAIR_SETACL:
	    case REPAIR_SETNACL:
	    case REPAIR_SETMODE:
	    case REPAIR_SETOWNER:
	    case REPAIR_SETMTIME:
		if (i >= 1) goto DoneParse;
		else break;

	    case REPAIR_CREATEF:
	    case REPAIR_CREATED:
	    case REPAIR_CREATEL:
	    case REPAIR_CREATES:
		if (i >= 3) goto DoneParse;
		else break;

	    case REPAIR_RENAME:
		if (i >= REPAIR_MAX) goto DoneParse;
		else break;
		
	    default: CODA_ASSERT(0); /* better not be anything else! */
	    }
	    
	/* Find the parameter */
	ADVANCE();
	NEXTFIELD();

	/* And convert it */
	if ((rs->opcode == REPAIR_SETACL) || (rs->opcode == REPAIR_SETNACL))
	    {if (acldecode(c, &rs->parms[i]) < 0) return(-1);}
	else if (rs->opcode == REPAIR_SETOWNER)
	    sscanf(c, "%d", &rs->parms[i]);
	else
	    sscanf(c, "%x", &rs->parms[i]);
    	}

DoneParse:
    /* Check no garbage at end of line */
    c = ++d;
    if (eos - c > 0)
	{
	c = eatwhite(c);
	if (*c) return(-1);
	}
    return(0);
    }

int repair_parsefile(char *fname, int *hdcount, struct listhdr **hdarray)
    /*
	fname		ascii input file
	hdcount		OUT: number of replicas
	hdarray		OUT: repair data structure

	Reads in ASCII file fname, parses each line, and constructs hdarray.
	
	Returns 0 on success, after allocating and constructing hdarray.
	Returns -1 with err msg if errors (including syntax errors) were found.
    */
	
    {
    int rc, lineno;
    char line[MAXPATHLEN];
    FILE *rf;
    struct repair rentry;


    rf = fopen(fname, "r");
    if (!rf) {perror(fname); return(-1);}

    *hdcount = 0;
    *hdarray = 0;
    lineno = 0;

    while (1)
	{
	/* get the next line */
	fgets(line, sizeof(line), rf);
	if (feof(rf)) break;
	lineno++;

	/* parse it */
	line[strlen(line)-1] = 0; /* nuke trailing \n */
	rc = repair_parseline(line, &rentry);
	switch(rc)
	    {
	    case 0: /* good line */
		break; 	    

	    case -2: /* blankline */
		continue;

	    case -1:
		fprintf(stderr, "%s: Syntax error, line %d\n", fname, lineno);
		fclose(rf);
		return(-1);
	    }
	    
	if (rentry.opcode == REPAIR_REPLICA) 	    
	    {/* new replica */

	    growarray((char **)hdarray, hdcount, sizeof(struct listhdr));
	    (*hdarray)[*hdcount - 1].replicaId = rentry.parms[0];
	    (*hdarray)[*hdcount - 1].repairCount = 0;
	    (*hdarray)[*hdcount - 1].repairList = 0;
	    }
	else
	    {/* another entry for the current replica */
	    struct repair **rearray;
	    int *recount;
	    
	    rearray = &((*hdarray)[(*hdcount - 1)].repairList);
	    recount  = (int *)&((*hdarray)[(*hdcount - 1)].repairCount);
	    growarray((char **)rearray, recount, sizeof(struct repair));
	    (*rearray)[*recount - 1] = rentry; /* struct assignment */
	    }
	}

    /* Done ! */
    fclose(rf);
    return(0);
    }




static int growarray(char **arrayaddr /* INOUT */, int *arraysize /* INOUT */, int elemsize)
    {
    *arraysize += 1; /* grow by one elem */
    if (*arraysize > 1)
    	*arrayaddr = (char *)realloc(*arrayaddr, (*arraysize)*elemsize);
    else
    	*arrayaddr = (char *)malloc(elemsize);    
    CODA_ASSERT(*arrayaddr); /* better not run out of memory */
    return(0);
    }




static char *eatwhite(char *s)
    /* Returns pointer to first non-white char, starting at s.
       Terminating null treated as non-white char.
    */
    {
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return(s);
    }

static char *eatnonwhite(char *s)
    /*  Returns pointer to first white char, starting at s.
        Terminating null treated as white char
    */
    {
    while (*s && !(*s == ' ' || *s == '\t')) s++;
    return(s);
    }


static int acldecode(char *s, unsigned int *r)
    /*
	s	input string
	r	output rights mask

	Decodes rights specified as [rliwdka] in s and returns rights mask in r.
	Returns 0 on success, -1 if s is bogus in some way
    */
    {
    register int i, max;

    if (!strcmp(s, "none")) s = "";
    if (!strcmp(s, "all")) s = "rlidwka";
    
    max = strlen(s);
    *r = 0;
    if (max == 1 && *s == '0')
	return(0);
   
    for (i = 0; i < max; i++)
	{
	switch(s[i])
	    {
	    case 'r': *r |= PRSFS_READ; break;
	    case 'w': *r |= PRSFS_WRITE; break;
	    case 'i': *r |= PRSFS_INSERT; break;
	    case 'l': *r |= PRSFS_LOOKUP; break;
	    case 'd': *r |= PRSFS_DELETE; break;
	    case 'k': *r |= PRSFS_LOCK; break;
	    case 'a': *r |= PRSFS_ADMINISTER; break;

	    default: return(-1);  /* bogus input */
	    }
	}
    return(0);
    }



void repair_printline(struct repair *rs, FILE *ff)
    {
    char *c;
    int i;
    
    switch(rs->opcode)
	{
	case REPAIR_CREATEF: c = "createf"; break;
	case REPAIR_CREATED: c = "created"; break;
	case REPAIR_CREATES: c = "creates"; break;
	case REPAIR_CREATEL: c = "createl"; break;
	case REPAIR_REMOVEFSL:  c = "removefsl"; break;
	case REPAIR_REMOVED:  c = "removed"; break;
	case REPAIR_SETACL:  c = "setacl"; break;
	case REPAIR_SETNACL:  c = "setnacl"; break;
	case REPAIR_SETMODE: c = "setmode"; break;
	case REPAIR_SETOWNER: c = "setowner"; break;
	case REPAIR_SETMTIME: c = "setmtime"; break;
        case REPAIR_RENAME: c = "mv"; break;
	default: c = "???????"; break;
	}
    

    fprintf(ff, "\t%s %s", c, rs->name);

    if (rs->opcode == REPAIR_RENAME) {
	fprintf(ff, "\t%s ", rs->newname);
	for (i = 0; i < REPAIR_MAX; i++)
	{
	    fprintf(ff, " %08x", rs->parms[i]);
	}
    }
    else if (rs->opcode != REPAIR_REMOVEFSL && rs->opcode != REPAIR_REMOVED)
	for (i = 0; i < REPAIR_MAX - 2 ; i++)
	{
	    fprintf(ff, " %08x", rs->parms[i]);
	}
    fprintf(ff, "\n");    
    }

void repair_printfile(char *fname) 
{
    int repcount;
    struct listhdr *list;
    int i, j;
    repair_getdfile(fname, &repcount, &list);
    for (i = 0; i < repcount; i++) {
	printf("New replica: volume id %x has %d repair entries\n",
	       list[i].replicaId, list[i].repairCount);
	for (j = 0; j < list[i].repairCount; j++) 
	    repair_printline(&list[i].repairList[j], stdout);
    }
}


