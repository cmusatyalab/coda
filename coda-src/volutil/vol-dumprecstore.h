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






/*
 * vol-dumprecstore.h
 * Created April, 1990
 * Author - Puneet Kumar 
 */

/*
 * Types for all of recoverable storage 
 */

#define	VOLHEADT	0
#define	SVNODEPTRARRT	1
#define	LVNODEPTRARRT	2
#define	SVNODEDISKPTRT	3
#define	LVNODEDISKPTRT	4
#define	VOLDISKDATAT	5
#define	DIRINODET	6
#define	DIRPAGET	7
#define	SFREEVNODEPTRARRT	9
#define	LFREEVNODEPTRARRT	10
#define	SFREEVNODEDISKPTRT	11
#define	LFREEVNODEDISKPTRT	12

typedef struct dumprec_t {
    char    *rec_addr;
    int	    size;
    short   type;
    short   index;
} dumprec_t;

