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

static char *rcsid = "$Header: /afs/cs.cmu.edu/project/coda-braam/src/coda-4.0.1/RCSLINK/./rvm-src/tests/testrvm.h,v 1.1 1996/11/22 19:18:01 braam Exp $";
#endif _BLURB_
/*
*
*
*                       Common Declarations for RVM Tests
*
*
*
*/

#define     MAP_DATA_FILE   "map_data_file"
#define     MAP_CHK_FILE    "map_chk_file"
#define     TEST_DATA_FILE  "test_data_file"
#define     T1_CHK_FILE     "t1_chk_file"
#define     T2_CHK_FILE     MAP_CHK_FILE
#define     T3_CHK_FILE     "t3_chk_file"
#define     T4_CHK_FILE     T3_CHK_FILE
#define     T5_CHK_FILE     "t5_chk_file"

#define     LOG_FILE        "log_file"


#define     T1_S1           100
#define     T1_L1           100
#define     T1_V1           0


#define     T3_S1           300
#define     T3_L1           50
#define     T3_V1           0100

#define     T3_S2           400
#define     T3_L2           150
#define     T3_V2           0101

#define     T3_S3           1000
#define     T3_L3           512
#define     T3_V3           0102


#define     T4_S1           2000
#define     T4_L1           150
#define     T4_V1           0141

#define     T4_S2           2100
#define     T4_L2           50
#define     T4_V2           0142

#define     T4_S3           2050
#define     T4_L3           500
#define     T4_V3           0143


#define     T5_S1           T4_S1
#define     T5_L1           T4_L1
#define     T5_V1           T4_V1

#define     T5_S2           T4_S2
#define     T5_L2           T4_L2
#define     T5_V2           T4_V2

#define     T5_S3           T4_S3
#define     T5_L3           T4_L3
#define     T5_V3           T4_V3

#define     T5_S4           2400
#define     T5_L4           100
#define     T5_V4           0144

#define     T5_S5           2600
#define     T5_L5           200
#define     T5_V5           0145

#define     T5_S6           2600
#define     T5_L6           100
#define     T5_V6           0146
