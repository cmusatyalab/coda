#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/rp2gen/test.c,v 1.1.1.1 1996/11/22 19:08:55 rvb Exp";
#endif /*_BLURB_*/


/*

                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.    This  code is provded "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to distribute this code, which is based on Version 2 of AFS
and  does  not  contain the features and enhancements that are part of
Version 3 of AFS.  Version 3 of  AFS  is  commercially  available  and
supported by Transarc Corporation, Pittsburgh, PA.

*/


#include "test.h"

static char *colors[] = { "red", "white", "blue" };

static pcbs(bs)
    RPC2_CountedBS *bs;
{
    register int i;

    printf("<%d, ", bs->SeqLen);
    for (i=0; i<bs->SeqLen; i++) printf("\\%02x", bs->SeqBody[i]);
    putchar('>');
}

static pbbs(bs)
    RPC2_BoundedBS *bs;
{
    register int i;

    printf("< %d, %d, ", bs->MaxSeqLen, bs->SeqLen);
    for (i=0; i<bs->SeqLen; i++) printf("\\%02x", bs->SeqBody[i]);
    putchar('>');
}

main(argc, argv)
    int argc;
    char *argv[];
{
    RPC2_Integer value;
    int cid = -2;
    static RPC2_String string;
    RPC2_String name[100];
    color tint;
    RPC2_CountedBS bs;
    RPC2_BoundedBS bbs;
    garbage g1, g2, g3;

    puts("[Test 1]");
    value = 1;
    printf("Calling proc1(%d, %d);\n", cid, value);
    printf("Returns: %d\n", proc1(cid, value));

    cid = 1234;
    string = (RPC2_String) "Eat me raw!";
    printf("Calling proc2(%d, \"%s\", ...);\n", cid, string);
    printf("Returns %d\n", proc2(cid, string, &value));
    printf("Returning, value = %d\n", value);

    cid = -567765;
    bbs.MaxSeqLen = 30;
    bbs.SeqLen = 0;
    bbs.SeqBody = (RPC2_Byte *) malloc(30);
    strcpy(name, "Alfred E. Newman");
    tint = blue;
    bs.SeqLen = 10;
    bs.SeqBody = (RPC2_Byte *) malloc(10);
    bcopy("0123456789", bs.SeqBody, 10);
    printf("Calling proc3(%d, ", cid);
    pbbs(&bbs);
    printf(", %s, \"%s\", ", colors[(int) tint], name);
    pcbs(&bs);
    puts(");");
    printf("Returns %d\n", proc3(cid, &bbs, &tint, name, &bs));
    printf("Returning from proc3, name = \"%s\", tint = %s, bs=", name, colors[(int) tint]);
    pcbs(&bs);
    printf(", bbs = ");
    pbbs(&bbs);
    putchar('\n');

    cid = 6523465;
    g1.code = 1;
    g1.place.x = 2;
    g1.place.y = 3;
    g1.time = 5764;
    g3.code = 4;
    g3.place.x = 5;
    g3.place.y = 6;
    g3.time = 86724371;
    printf("Returns %d\n", proc4(cid, &g1, &g2, &g3));
}

int test_proc1(cid, n)
    RPC2_Handle cid;
    int n;
{
    printf("Entering proc1(%d, %d);\n", cid, n);
    return -1;
}

int test_proc2(cid, s, n)
    RPC2_Handle cid;
    char *s;
    int *n;
{
    printf("Entering proc2(%d, \"%s\", ...);\n", cid, s);
    *n = 87654321;
    return 2;
}

int test_proc3(cid, bbs, c, s, bs)
    RPC2_Handle cid;
    RPC2_BoundedBS *bbs;
    color *c;
    RPC2_String s;
    RPC2_CountedBS *bs;
{
    printf("Entering proc3(%d, ", cid);
    pbbs(bbs);
    printf(", %s, \"%s\", ", colors[(int) *c], s);
    pcbs(bs);
    puts(");");
    bbs->SeqLen = 4;
    strcpy(bbs->SeqBody, "ABCD");
    s[7] = (RPC2_Byte) '?';
    *c = white;
    bs->SeqBody[0] = (RPC2_Byte) '!';
    return 333;
}

int test_proc4(cid, g1, g2, g3)
    RPC2_Handle cid;
    garbage *g1, *g2, *g3;
{
    g2->code = 7;
    g2->place.x = 8;
    g2->place.y = 9;
    g2->time = 6324512;
    g3->code = 999;
    g3->place.x = 998;
    g3->place.y = 997;
    g3->time = 0;
    return 4;
}

int RPC2_AllocBuffer(size, buff)
    int size;
    RPC2_PacketBuffer **buff;
{
    *buff = (RPC2_PacketBuffer *) malloc(sizeof(RPC2_PacketBuffer)+size-1);
    if (*buff != 0)
	return RPC2_SUCCESS;
    else
	return RPC2_FAIL;
}

int RPC2_FreeBuffer(buff)
    RPC2_PacketBuffer **buff;
{
    return RPC2_SUCCESS;
}

RPC2_PacketBuffer *answer;

int RPC2_MakeRPC(cid, req, bd, rsp, life, options)
    RPC2_Handle cid;
    RPC2_PacketBuffer *req, **rsp;
    int bd, life, options;
{
    test1_ExecuteRequest(cid, req, bd);
    *rsp = answer;
    return RPC2_SUCCESS;
}

int RPC2_SendResponse(cid, rsp, bd, life)
    RPC2_Handle cid;
    RPC2_PacketBuffer *rsp;
    int bd, life;
{
    answer = rsp;
}
