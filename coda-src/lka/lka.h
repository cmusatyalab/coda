/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.


              Copyright (c) 2002-2003 Intel Corporation

#*/

#ifndef _LKA_H_INCLUDED_
#define _LKA_H_INCLUDED_ 1

#include <openssl/sha.h>

/* "helper" routines in shaprocs.cc */
void ViceSHAtoHex (unsigned char sha[SHA_DIGEST_LENGTH], char *buf, int buflen);
int CopyAndComputeViceSHA(int infd, int outfd, unsigned char sha[SHA_DIGEST_LENGTH]);
#define ComputeViceSHA(fd, sha) CopyAndComputeViceSHA(fd, -1, sha)
int IsZeroSHA(unsigned char sha[SHA_DIGEST_LENGTH]);

/* "core" routines in lka.cc called by venus */
int LookAsideAndFillContainer(unsigned char sha[SHA_DIGEST_LENGTH], int cfd,
			      int length, char *venusRoot,
			      char *emsg, int emsglen);
int LKParseAndExecute(char *in, char *out, int len);

#endif /*_LKA_H_INCLUDED_ */
