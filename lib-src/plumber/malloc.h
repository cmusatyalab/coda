/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer:  $
*/






#ifndef _MALLOC_
#define _MALLOC_

/* for Sun and system V */
/*
	Constants defining mallopt operations
*/
#define M_MXFAST	1	/* set size of 'small blocks' */
#define M_NLBLKS	2	/* set num of small blocks in holding block */
#define M_GRAIN		3	/* set rounding factor for small blocks */
#define M_KEEP		4	/* (nop) retain contents of freed blocks */

/*
	malloc information structure
*/
struct mallinfo  {
	int arena;	/* total space in arena */
	int ordblks;	/* number of ordinary blocks */
	int smblks;	/* number of small blocks */
	int hblks;  	/* number of holding blocks */
	int hblkhd;	/* space in holding block headers */
	int usmblks;	/* space in small blocks in use */
	int fsmblks;	/* space in free small blocks */
	int uordblks;	/* space in ordinary blocks in use */
	int fordblks;	/* space in free ordinary blocks */
	int keepcost;	/* cost of enabling keep option */

	int mxfast;	/* max size of small blocks */
	int nlblks;	/* number of small blocks in a holding block */
	int grain;	/* small block rounding factor */
	int uordbytes;	/* space (including overhead) allocated in ord. blks */
	int allocated;	/* number of ordinary blocks allocated */
	int treeoverhead;	/* bytes used in maintaining the free tree */
};

/* the following routines are defined by the ITC malloc and do reasonable things */
extern int  	malloc_debug(/* level */);
extern int  	malloc_verify();

/* _malloc_at_addr is a system V call;  for now it always returns zero */
extern char	*_malloc_at_addr(/* addr, size */);

/* these routines exist only on Sun4.3 v3.4;  with the ITC malloc they do nothing */	
extern int  		mallopt();
extern struct mallinfo  	mallinfo();

/* the following extern variables are defined in malloc.c, but are given only dummy values:
		 __mallinfo, _root, _lbound, _ubound
*/

/* end of Sun and System V stuff */

#define MAXQUEUE 3

struct arenastate {
	struct freehdr *arenastart;	/* point to first block in arena */
	struct freehdr *arenaend;	/* point to the segment trailer for last segment */
	struct freehdr *allocp;	/*free list ptr*/
	int SeqNo;		/* stored by plumber */
	char arenahasbeenreset;	/* 1 after a reset */
	char InProgress;		/* 1 during malloc, realloc, and free */
	char RecurringM0;		/* 1 when recurring due to request too large */
	char MallocStrategy;	/* see below */
	int NQueued;	/* the number of occupied elts in QueuedToFree */
		/* the next field is at the end and is not initialized */
	struct hdr *QueuedToFree[MAXQUEUE];
};

#ifndef _IBMR2
#ifndef __STDC__
extern char  	*malloc(/* size */);
extern char  	*realloc(/* block, newsize */);
extern char	*calloc(/* int numblks, int size */);
#endif /* __STDC__ */
#endif /* _IBMR2 */
extern void  	free(/* block */);
extern void	cfree(/* char *block, int numblks, int size */);
extern long  	CheckAllocs(/* char *callername */);
extern void  	mstats(/* char *callername */);
extern void  	MallocStats(/* char *callername, FILE *file*/);
extern void  	resetmstats();
extern int  	(*SetM0Handler(/* int (*proc()) */))();
extern int  	SetMallocCheckLevel(/* int level */);
extern struct arenastate  	*GetMallocArena();
extern void 	NewMallocArena();
extern int  	MallocStrategy(/* int strategy */);
extern void  	AbortFullMessage(/* long sizethatfailed */);
extern void  	plumber(/* FILE *file */);

/* values which can be OR'ed to form MallocStrategy */
#define AllocFront 1
#define DelayAdvance 2
#define AdvanceKludge 4

#endif /* _MALLOC_ */
