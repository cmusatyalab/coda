/* ********************************************************************** *\
 *         Copyright IBM Corporation 1991 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */

/*
	$Disclaimer:  $
*/






/* In routine f the statement
	GETCALLER(a, x)
   where a is the first parater to f and x is of type (char *)
   will store in x the return address within the caller of f.  
   This cannot be guaranteed to always work,
   so it cannot be recommended for anything other than diagnostic infomation.
*/


#ifndef _GETCALLR_
#define _GETCALLR_


#ifdef ibm032
#define RETADDROFF (6)
#else /* ! ibm032 */

#ifdef _IBMR2

#define RETADDROFF (4)

#else	/* ! _IBMR2  &&  ! ibm032 */

#define RETADDROFF (1)

#endif /* ! _IBMR2 */
#endif /* ! ibm032 */


#if (defined(sun4) || defined(sparc))

	/* the sun4 requires an assembler routine to find the caller of the caller */

#define GETCALLER(a, x) 	{extern char *getcaller(); x = getcaller();}

#else /* (defined(sun4) || defined(sparc)) */
#if (defined(mips) || defined(pmax))

#define GETCALLER(a, x)    {extern char *store_return_address; x = store_return_address;}

#else  /* defined(mips) || defined(pmax) */

	/* the vax compilers require the extra variable 'addrloc'
	others do not, but there is no great advantage in making a third case.  */

#define GETCALLER(a, x) 	{  \
	register char **addrloc;  \
	addrloc = (((char **)&a) - RETADDROFF);  \
	x = *addrloc;  \
}

#endif /* defined(mips) || defined(pmax) */
#endif /* (defined(sun4) || defined(sparc)) */

#if (defined(mips) || defined(pmax))
#if __STDC__
#define procdef(procname) procname##_sidedoor
#else
#define procdef(procname) procname/**/_sidedoor
#endif
#else /* (defined(mips) || defined(pmax)) */
#define procdef(procname) procname
#endif /* (defined(mips) || defined(pmax)) */


#endif  /* _GETCALLR_ */

