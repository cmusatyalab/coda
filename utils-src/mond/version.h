#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
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

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/utils-src/mond/version.h,v 3.3 2003/05/23 18:27:58 jaharkes Exp $";
#endif /*_BLURB_*/



#ifndef VERSION_H
#define VERSION_H

class connection_entry : public olink {
    RPC2_Handle myCid;
    long        myClientType;
public:
    connection_entry(RPC2_Handle, long);
    long client_type(void) {return myClientType;}
    RPC2_Handle cid(void) {return myCid;}
    void print();
    void print(FILE *);
    void print(int);
    void print(int,FILE*);
};

class connection_table {
    friend long MondEstablishConn(RPC2_Handle, unsigned long, 
				  long, long, SpareEntry[]);
    ohashtab *table;
public:
    connection_entry *GetConnection(RPC2_Handle);
    int ConnectionValid(RPC2_Handle, long);
    int RemoveConnection(RPC2_Handle);
    void LogConnections(int,FILE*);
    int PurgeConnections(void);
    connection_table(int =1024);
    ~connection_table(void);
};

char *HostNameOfConn(RPC2_Handle);
extern connection_table *ClientTable;

#endif VERSION_H
