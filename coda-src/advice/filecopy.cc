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

/*  filecopy  --  copy a file from here to there
 *
 *  Usage:  i = filecopy (here,there);
 *	int i, here, there;
 *
 *  Filecopy performs a fast copy of the file "here" to the
 *  file "there".  Here and there are both file descriptors of
 *  open files; here is open for input, and there for output.
 *  Filecopy returns 0 if all is OK; -1 on error.
 *
 *  I have performed some tests for possible improvements to filecopy.
 *  Using a buffer size of 10240 provides about a 1.5 times speedup
 *  over 512 for a file of about 200,000 bytes.  Of course, other
 *  buffer sized should also work; this is a rather arbitrary choice.
 *  I have also tried inserting special startup code to attempt
 *  to align either the input or the output file to lie on a
 *  physical (512-byte) block boundary prior to the big loop,
 *  but this presents only a small (about 5% speedup, so I've
 *  canned that code.  The simple thing seems to be good enough.
 *
 *  HISTORY
 * $Log: filecopy.cc,v $
 * Revision 4.2  1998/12/21 22:24:58  jaharkes
 * Here it is.... GPL/LGPL
 *
 * Revision 4.1  1997/01/08 21:49:16  rvb
 * r = 4.1; fix $ HEADERS
 *
 * Revision 1.1  1996/11/22 19:12:12  braam
 * First Checkin (pre-release)
 *
Revision 1.1  96/11/22  13:33:59  raiff
First Checkin (pre-release)

 * Revision 1.1.3.1  96/08/26  12:09:00  raiff
 * Branch for release beta-26Aug1996_41240
 * 
 * Revision 1.1  96/04/05  09:38:23  mre
 * Initial revision
 * 
 * Revision 1.2  90/12/11  17:52:57  mja
 * 	Add copyright/disclaimer for distribution.
 * 
 * 20-Nov-79  Steven Shafer (sas) at Carnegie-Mellon University
 *	Rewritten for VAX; same as "filcopy" on PDP-11.  Bigger buffer
 *	size (20 physical blocks) seems to be a big win; aligning things
 *	on block boundaries seems to be a negligible improvement at
 *	considerable cost in complexity.
 *
 * 27-Mar-96  Maria Ebling (mre) at Carnegie Mellon University
 *      Use a smaller buffer (1K rather than 10K) so we don't overrun 
 *      the LWP stack!
 */

#define BUFFERSIZE 1024

#ifdef __cplusplus
extern "C" {
#endif __cplusplus
extern int read(int, char *, int);
extern int write(int, char *, int);
#ifdef __cplusplus
}
#endif __cplusplus

int myfilecopy(int fc_here, int fc_there)
{
	register int kount;
	char fc_buffer[BUFFERSIZE];
	kount = 0;
	while (kount == 0 && (kount=read(fc_here,fc_buffer,BUFFERSIZE)) > 0)
		kount -= write(fc_there,fc_buffer,kount);
	return (kount ? -1 : 0);
}
