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
 * slow.h -- include file for network delay package
 *           L. Mummert
 */
#define MINDELAY  16   /* minimum delay time, in msec */
 		       /* anything less is not delayed */


/* Exported Routines */
int Delay_Init ();
int DelayPacket (int speed, long socket, struct sockaddr_in *sap, RPC2_PacketBuffer *pb, int queue);
int FindQueue (unsigned char a, unsigned char b, unsigned char c, unsigned char d);
int MakeQueue (unsigned char a, unsigned char b, unsigned char c, unsigned char d);
     

     
