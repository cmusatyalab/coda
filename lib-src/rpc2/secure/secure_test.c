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

#include <stdlib.h>
#include <rpc2/secure.h>

/* This is clearly minimal, we already run tests during initialization */
int main(int argc, char **argv)
{
    secure_init(1);
    secure_release();
    return EXIT_SUCCESS;
}

