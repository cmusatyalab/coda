#ifndef _BLURB_
#define _BLURB_
/*
 * This code was originally part of the CMU SCS library "libcs".
 * A branch of that code was taken in May 1996, to produce this
 * standalone version of the library for use in Coda and Odyssey
 * distribution.  The copyright and terms of distribution are
 * unchanged, as reproduced below.
 *
 * Copyright (c) 1990-96 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND CARNEGIE MELLON UNIVERSITY
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT
 * SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Users of this software agree to return to Carnegie Mellon any
 * improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * Export of this software is permitted only after complying with the
 * regulations of the U.S. Deptartment of Commerce relating to the
 * Export of Technical Data.
 */

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/lib-src/libcs/Attic/ci.h,v 4.1 1997/01/08 21:53:48 rvb Exp $";
#endif /*_BLURB_*/

/*
 * macros and declarations for ci routine
 * 
 * BUGS
 * Must have literals for cluster prompts, cchr legal character set,
 * 	entry names.
 */

/*
 * Usage:  ci (prompt, file, depth, list, helppath, cmdfpath);
 */
/*
 * Name conventions:
 *  <type> is either:
 * 	<simpletype>		int, etc.; a simple variable
 * 	c<simpletype>		cint, etc.; a cluster variable
 *  ci_<type>		a structure declaration
 *  ci_u<type>		a union element of ci_union
 *  ci_t<type>		a type identifier of ci_type
 *  <TYPE>		macro for declaring a clustered type
 *  CI<TYPE>		macro for element of ci entry list
 * 
 * <simpletype> is one of:
 * 	int	long	short	oct	hex	double	float
 * 	chr	string	stab	search
 * with various specials (cmd, proc, end) thrown in.
 */

/*  For internal use by ci only:  Clustered type structures */

typedef struct {	/* int */
	int	*ci_ival;		/* ptr to int (current value) */
	int	ci_imin, ci_imax;	/* min and max allowed values */
	char	*ci_imsg;		/* prompt message */
} ci_cint;

typedef struct {	/* short */
	short	*ci_sval, ci_smin, ci_smax;
	char	*ci_smsg;
} ci_cshort;

typedef struct {	/* long */
	long	*ci_lval, ci_lmin, ci_lmax;
	char	*ci_lmsg;
} ci_clong;

typedef struct {	/* octal and hex */
	unsigned int	*ci_uval, ci_umin, ci_umax;
	char	*ci_umsg;
} ci_cunsigned;

typedef struct {	/* double */
	double	*ci_dval, ci_dmin, ci_dmax;
	char	*ci_dmsg;
} ci_cdouble;

typedef struct {	/* float */
	float	*ci_fval, ci_fmin, ci_fmax;
	char	*ci_fmsg;
} ci_cfloat;

typedef struct {	/* boolean */
	int	*ci_bval;	/* ptr to value */
	char	*ci_bmsg;	/* prompt message */
} ci_cbool;

typedef struct {	/* chr */
	int	*ci_cval;	/* ptr to int value */
	char	*ci_cleg;	/* string of legal chars */
	char	*ci_cmsg;	/* prompt message */
} ci_cchr;

typedef struct {	/* string */
	char	*ci_pval;	/* ptr to string */
	int	ci_plen;	/* length */
	char	*ci_pmsg;	/* prompt message */
} ci_cstring;

typedef struct {	/* string table */
	int	*ci_tval;	/* ptr to value */
	char	**ci_ttab;	/* table */
	char	*ci_tmsg;	/* prompt message */
} ci_cstab;

/* FOR USERS: DECLARATIONS OF INSTANCES OF CLUSTERED TYPES */

#define CINT(str,var,mn,mx,msg)		int var; ci_cint str={&var,mn,mx,msg}
#define CSHORT(str,var,mn,mx,msg)	short var; ci_cshort str={&var,mn,mx,msg}
#define CLONG(str,var,mn,mx,msg)	long var; ci_clong str={&var,mn,mx,msg}
#define COCT(str,var,mn,mx,msg)		unsigned int var; ci_cunsigned str={&var,mn,mx,msg}
#define CHEX(str,var,mn,mx,msg) 	unsigned int var; ci_cunsigned str={&var,mn,mx,msg}
#define CDOUBLE(str,var,mn,mx,msg)	double var; ci_cdouble str={&var,mn,mx,msg}
#define CFLOAT(str,var,mn,mx,msg)	float var; ci_cfloat str={&var,mn,mx,msg}
#define CBOOL(str,var,msg)		int var; ci_cbool str={&var,msg}
#define CCHR(str,var,leg,msg)		int var; ci_cchr str={&var,leg,msg}
#define CSTRING(str,var,len,msg)	char var[len]; ci_cstring str={var,len,msg}
#define CSTAB(str,var,tbl,msg)		int var; ci_cstab str={&var,tbl,msg}
#define CSEARCH(str,var,tbl,msg)	int var; ci_cstab str={&var,tbl,msg}

/*  For internal use in ci only:  Union of entry types */

typedef union {
	int		ci_uint;
	short		ci_ushort;
	long		ci_ulong;
	unsigned int	ci_uoct;
	unsigned int	ci_uhex;
	double		ci_udouble;
	float		ci_ufloat;
	int		ci_ubool;
	char		*ci_ustring;
	int		(*ci_uproc)();	/* variable procedures */
	ci_cint		ci_ucint;
	ci_cshort	ci_ucshort;
	ci_clong	ci_uclong;
	ci_cunsigned	ci_ucoct;
	ci_cunsigned	ci_uchex;
	ci_cdouble	ci_ucdouble;
	ci_cfloat	ci_ucfloat;
	ci_cbool	ci_ucbool;
	ci_cchr		ci_ucchr;
	ci_cstring	ci_ucstring;
	ci_cstab	ci_ucstab;
	ci_cstab	ci_ucsearch;
	int		(*ci_ucmd) ();	/* a command, not a variable */
} ci_union;

/* For internal use in ci only:  Type identifiers */

typedef enum {
	ci_tint,
	ci_tshort,
	ci_tlong,
	ci_toct,
	ci_thex,
	ci_tdouble,
	ci_tfloat,
	ci_tbool,
	ci_tstring,
	ci_tproc,
	ci_tclass,
	ci_tcint,
	ci_tcshort,
	ci_tclong,
	ci_tcoct,
	ci_tchex,
	ci_tcdouble,
	ci_tcfloat,
	ci_tcbool,
	ci_tcchr,
	ci_tcstring,
	ci_tctab,
	ci_tcsearch,
	ci_tcmd,	/* a command, not a variable */
	ci_tend		/* the end of the ci entry list */
} ci_type;

/* FOR USERS: AN ENTRY ON THE ENTRY LIST */

typedef struct {
	char		*ci_enam;	/* the name of the entry */
	ci_union	*ci_eptr;	/* a ptr to the value */
	ci_type		ci_etyp;	/* the type of the entry */
	char		*ci_evar;	/* ptr to var for CICLASS */
} CIENTRY;

/* FOR USERS: THE ENTRIES OF THE ENTRY LIST */

#define CIINT(n,i)	{n, (ci_union *)&(i), ci_tint, 0}
#define CISHORT(n,s)	{n, (ci_union *)&(s), ci_tshort, 0}
#define CILONG(n,l)	{n, (ci_union *)&(l), ci_tlong, 0}
#define CIOCT(n,o)	{n, (ci_union *)&(o), ci_toct, 0}
#define CIHEX(n,h)	{n, (ci_union *)&(h), ci_thex, 0}
#define CIDOUBLE(n,d)	{n, (ci_union *)&(d), ci_tdouble, 0}
#define CIFLOAT(n,f)	{n, (ci_union *)&(f), ci_tfloat, 0}
#define CIBOOL(n,b)	{n, (ci_union *)&(b), ci_tbool, 0}
#define CISTRING(n,s)	{n, (ci_union *)(s),  ci_tstring, 0}
#define CIPROC(n,p)	{n, (ci_union *)(p),  ci_tproc, 0}
#define CICLASS(n,v,p)	{n, (ci_union *)(p), ci_tclass, (char *)&(v)}
#define CICINT(n,ci)	{n, (ci_union *)&(ci), ci_tcint, 0}
#define CICSHORT(n,cs)	{n, (ci_union *)&(cs), ci_tcshort, 0}
#define CICLONG(n,cl)	{n, (ci_union *)&(cl), ci_tclong, 0}
#define CICOCT(n,co)	{n, (ci_union *)&(co), ci_tcoct, 0}
#define CICHEX(n,ch)	{n, (ci_union *)&(ch), ci_tchex, 0}
#define CICDOUBLE(n,cd)	{n, (ci_union *)&(cd), ci_tcdouble, 0}
#define CICFLOAT(n,cf)	{n, (ci_union *)&(cf), ci_tcfloat, 0}
#define CICBOOL(n,cb)	{n, (ci_union *)&(cb), ci_tcbool, 0}
#define CICCHR(n,cc)	{n, (ci_union *)&(cc), ci_tcchr, 0}
#define CICSTRING(n,cs)	{n, (ci_union *)&(cs), ci_tcstring, 0}
#define CICSTAB(n,cs)	{n, (ci_union *)&(cs), ci_tctab, 0}
#define CICSEARCH(n,cs)	{n, (ci_union *)&(cs), ci_tcsearch, 0}
#define CICMD(n,p)	{n, (ci_union *)(p), ci_tcmd, 0}
#define CIEND		{0, 0,ci_tend,0}

/* FOR USERS:  VARIABLE PROCEDURE MODES */

typedef enum {
	CISET, CISHOW, CIPEEK
} CIMODE;

/* FOR USERS: GLOBAL VARIABLES */

extern FILE *ciinput;		/* FILE used for current ci input */
extern int ciquiet;		/* quiet bits (1 = quiet, 0 = noisy) */
extern int ciexit;		/* set this to 1 to cause ci to return */
extern char cinext[];		/* use this instead of reading file */
extern char ciprev[];		/* previous ci command */
extern int cidepth;		/* user access to current ci level */
extern int ciback;	/* jump back Loretta to command ciprev[] */

/* FOR USERS: QUIET BITS */

#define CISHEXIT	01
#define CISETPEEK	02
#define CICMDFECHO	04
#define CICMDFEXIT	010
#define CICMDFPROMPT	020
#define CICMDFPEEK	040
#define CICMDNOINDENT   0100
#define CINOSEM		0200
#define CIFIRSTEQUAL	0400
#define CINOFILE	01000

/* FOR USERS: PROTOTYPES */

#ifndef	__P
#if	__STDC__
#define __P(x) x
#else
#define __P(x) ()
#endif
#endif

extern void ci __P((char *, FILE *, int, CIENTRY *, char *, char *));
