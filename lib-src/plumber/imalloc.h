/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer:  $
*/






/*
 *	a different implementation may need to redefine
 *	INT,  WORD,  SIZEOFINT, and SIZEOFCHARSTAR
 *	where INT is an integer type to which a pointer can be cast
 *	and WORD is the boundary alignment required (typically 4 or 8)
 *	WORD needs to be at least 4 so there are two zero bits
 *	at the bottom of a Size field  for ACTIVE and PREACTIVE
 *	SIZEOFINT is sizeof(int)   SIZEOFCHARSTAR is sizeof(char *)
 *	(The last two are needed because sizeof() is illegal in #if)
 *	WORD must be a multiple of SIZEOFINT
 */
#ifndef _MALLOCITC_
#define _MALLOCITC_

#ifndef INT
#define INT long
#endif /* INT */

#ifndef WORD
	/* for SPARC the Makefile has "-DWORD=8" */
#define WORD 4
#endif /* WORD */

#ifndef SIZEOFINT
#define SIZEOFINT   4
#endif /* SIZEOFINT */

#ifndef SIZEOFCHARSTAR
#define SIZEOFCHARSTAR   4
#endif /* SIZEOFCHARSTAR */

#define SEGGRAIN  4096 /* granularity for sbrk requests (in bytes) */

#if WORD % SIZEOFINT
	WORD must be a multiple of SIZEOFINT
#endif
#if WORD < 4
	WORD must be 4 or more
#endif

#define EPSILON  ((sizeof(struct freehdr)+sizeof(struct freetrlr)+(WORD-1))/WORD*WORD)
#define ACTIVE    0x1
#define PREACTIVE 0x2
#define testbit(p, b) ((p)&(b))
#define setbits(p, b) ((p)|(b))
#define clearbits(p) ((p)&(~ACTIVE)&(~PREACTIVE))
#define clearbit(p, b) ((p)&~(b))
#define NEXTBLOCK(p) ((struct freehdr *)((INT)p+clearbits(p->Size)))
#define PREVFRONT(p) ((((struct freetrlr *)(p))-1)->Front)

#ifndef IDENTIFY

#if SIZEOFINT % WORD
#define PADHEADER   \
	int padding[(WORD - SIZEOFINT%WORD) / SIZEOFINT];
#else
#define PADHEADER
#endif

struct hdr {
	PADHEADER
	int Size;			/* header for active blocks; Size includes the header */
				/* the two low order bits of the Size fields 
				are used for ACTIVE and PREACTIVE */
};
struct freehdr {
	PADHEADER
	int Size;			/* size includes the header */
	struct freehdr *Next, *Prev; /* doubly linked circular list */
};
struct freetrlr {
	struct freehdr *Front;	/* last word in free block points to the freehdr */
};
struct segtrlr {
	PADHEADER
	int Size;			/* zero | ACTIVE */
	struct freehdr *Next, *Prev; /* doubly linked circular list */
	struct freehdr *Front;	/* points to beginning of segment */
};

#else /* IDENTIFY */

/* two additional words on every block identify the caller that created the block
   and it sequence number among all block creations */

#if (SIZEOFCHARSTAR + 2*SIZEOFINT) % WORD
#define PADHEADER   \
	int padding[(WORD-(SIZEOFCHARSTAR+2* SIZEOFINT)%WORD) / SIZEOFINT];
#else
#define PADHEADER
#endif

struct hdr { 
	PADHEADER
	char *caller;
	int seqno;
	int Size; 
				/* the two low order bits of the Size fields 
				are used for ACTIVE and PREACTIVE */
};
struct freehdr {
	PADHEADER
	char *caller;
	int seqno;
	int Size;
	struct freehdr *Next, *Prev;
};
struct freetrlr {
	struct freehdr *Front;
};
struct segtrlr {
	PADHEADER
	char *caller;		
	int seqno;
	int Size;
	struct freehdr *Next, *Prev;
	struct freehdr *Front;
};

#endif /* IDENTIFY */

#undef PADHEADER

#endif  /* _MALLOCITC_ */
