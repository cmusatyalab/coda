/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

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



/* This is the salvage test program. */

main (argc, argv)
    int argc;
    char **argv;
    {long ofid, nfid, code;
    DInit(20);
    if (argc == 2)
        {ofid = atoi(argv[1]);
        nfid = 0;
        }
    else if (argc == 3)
        {ofid = atoi(argv[1]);
        nfid = atoi(argv[2]);
        }
    else
        {printf("usage is: test <ofid> <optional new fid>\n");
        exit(1);
        }
    code = DirOK(&ofid);
    printf("DirOK returned %d.\n");
    if (nfid)
        {printf("Salvaging from fid %d into fid %d.\n", ofid, nfid);
        code = DirSalvage(&ofid, &nfid);
        printf("DirSalvage returned %d.\n", code);
        }
    DFlush();
    }

/* the end */
