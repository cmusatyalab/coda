/* BLURB lgpl
			Coda File System
			    Release 6

	    Copyright (c) 2006 Carnegie Mellon University
		  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

			Additional copyrights
#*/

#include <rpc2/secure.h>
#include "aes.h"
#include "grunt.h"

void secure_init(int verbose)
{
    /* Initialize and run the AES test vectors */
    secure_aes_init(verbose);

    /* Initialize and test the PRNG */
    secure_random_init(verbose);
}

void secure_release(void)
{
    secure_random_release();
}

