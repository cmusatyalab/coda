/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#ifndef _LWP_TIMER_H_
#define _LWP_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

struct TM_Elem {
    struct TM_Elem	*Next;		/* filled by package */
    struct TM_Elem	*Prev;		/* filled by package */
    struct timeval	TotalTime;	/* filled in by caller; 
					   changed to expiration by package */
    struct timeval	TimeLeft;	/* filled by package */
    char		*BackPointer;	/* filled by caller, not interpreted by package */
};


#define FOR_ALL_ELTS(var, list, body)\
	{\
	    register struct TM_Elem *_LIST_, *var, *_NEXT_;\
	    _LIST_ = (list);\
	    for (var = _LIST_ -> Next; var != _LIST_; var = _NEXT_) {\
		_NEXT_ = var -> Next;\
		body\
	    }\
	}

/* extern definitions of timer routines */
extern void TM_Insert (struct TM_Elem *tlistPtr, struct TM_Elem *elem);
extern void TM_Remove (struct TM_Elem *tlistPtr, struct TM_Elem *elem);
extern int  TM_Rescan (struct TM_Elem *tlist);
extern struct TM_Elem *TM_GetExpired (struct TM_Elem *tlist);
extern struct TM_Elem *TM_GetEarliest (struct TM_Elem *tlist);

extern int  TM_eql (register struct timeval *t1, register struct timeval *t2);
extern int  TM_Init (register struct TM_Elem **list);
extern int  TM_Final (register struct TM_Elem **list);
extern void TM_Insert (struct TM_Elem *tlistPtr, struct TM_Elem *elem);
extern int  TM_Rescan (struct TM_Elem *tlist);
extern struct TM_Elem *TM_GetExpired (struct TM_Elem *tlist);
extern struct TM_Elem *TM_GetEarliest (struct TM_Elem *tlist);

#ifdef __cplusplus
}
#endif __cplusplus

#endif /* _LWP_TIMER_H_ */
