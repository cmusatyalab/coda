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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/timing_clock.c,v 1.1.1.1 1996/11/22 19:08:21 rvb Exp";
#endif /*_BLURB_*/







#if defined(romp) || defined(ibm032) || defined(ibmrt)
/*
 *  The following routines allow a user level program to read the
 *  counter/timer registers from the DT2806 board without using the
 *  device driver.  (Hence, without using a system call.)
 *
 *  The correct use of these routines requires that the user first
 *  call the open_timing_clock() routine.  Once this has completed
 *  successfully, the user has exclusive access of the DT2806 board.
 *  At this point, the user may call the read_timing_clock() routine
 *  any number of times to receive the current time.  When the user
 *  is finished, s/he MUST call the close_timing_clock() routine.
 *  This routine releases exclusive access of the DT2806 board and
 *  returns control of the board to the kernel.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "dtcreg.h"

#define DTCLOWMASK(n)	(n & 0xFF)
#define DTCMASK(n)	(n & 0xFFFF)

#define	MAXTRIES	10

long dtcsec, dtcusec;
struct timeval basetime;

struct dtc_counters {
   long timer2, timer1, timer0
};

/* open_timing_clock()
 *     The open_timing_clock routine opens /dev/bus.  Opening 
 *     /dev/bus allows the user to access addresses on the bus
 *     as if they were memory.  This routine must be called once
 *     before the read_timing_clock() routine is called to ensure
 *     mutually exclusive access to the clock.  In addition, the
 *     close_timing_clock() routine should be called before 
 *     exiting to release control of the board to the kernel.
 */
int open_timing_clock(bus_ptr, dtc_ptr)
int *bus_ptr, *dtc_ptr;
{
  int rc;

  /* Open the DT2806 board */
  *dtc_ptr = open("/dev/dtc0",O_RDONLY,0);
  if (*dtc_ptr < 0)
  {
    perror("Open of /dev/dtc0 failed.\n");
    return(-1);
  }

  /* Call the dtc driver to gain exclusive access to /dev/dtc0 */
  rc = ioctl(*dtc_ptr, DTCRLS, &basetime);
  if (rc != 0)
  {
    perror("Cannot obtain exclusive access to the clock board.\n");
    return(rc);
  }

  /* Now open the bus so that we can access registers on the board. */
  if ((*bus_ptr = open("/dev/bus",O_RDONLY,0)) < 0) 
  {
    perror("Open of /dev/bus failed\n");
    return(*bus_ptr);
  }

  return(1);
}

/* close_timing_clock()
 *     The close_timing_clock() routine closes the file descriptor
 *     provided to it (should be that of /dev/bus) and releases
 *     control of /dev/dtc0 to the kernel.  This routine should be
 *     called before exiting the calling program because otherwise
 *     the mutual exclusion will not be released (preventing other
 *     processes access to the accurate timing clock). 
 */    
close_timing_clock(bus_ptr,dtc_ptr)
  int bus_ptr,dtc_ptr;
{
  /* Close the bus */
  close(bus_ptr); 

  /* Call the dtc driver to return control of /dev/dtc0 to the kernel */
  ioctl(dtc_ptr, DTCCTL);
}

/* read_timing_clock()
 *     The read_timing_clock() routine reads the values of the
 *     counter/timers directly from the DT2806 board as if the
 *     registers on the board were memory.  This prevents us 
 *     from having to use a system call.
 *
 *     The routine assumes that a cascade COULD occur in the
 *     middle of the reading and checks to make sure that such
 *     a time does not get reported to the user.  To ensure this,
 *     the routine reads the counter/timer values twice requiring
 *     the two most significant counter/timers to have the same
 *     values between reads (eg, value1.timer2 == value2.timer2)
 *     In addition, the routine requires that the second value
 *     of the least significant counter be greater than the first
 *     value of the least significant counter.  (This ensure a
 *     monitonically increasing time values on consecutive reads.)
 */ 
read_timing_clock(time_value)
struct timeval *time_value;
{
  int counter, correct_read;
  struct dtc_counters value1, value2;
  struct dtcclk data;
  volatile long tempa, tempb;
  char *modereg = ModeReg;
 
  counter = 0;
  correct_read = 0;
  while ( (!correct_read) && (counter < MAXTRIES) )
  {
    /* Read the clock registers from the board.
     *
     * Dereference the Timer ptrs to get the clock values 
     *
     * tempa gets the low 8 bits. tempb gets the high 8 bits. 
     *
     * Write appropriate values to the Mode Register to 
     * latch the current value in the timer in a storage
     * register -- see 5-3 of DT2806 manual 
     */
    *modereg = 0x80;
    tempa =  DTCLOWMASK(*Timer2); 
    tempb =  DTCLOWMASK(*Timer2);
    value1.timer2 = DTCMASK(0x10000 - ((tempb << 8)|tempa));
    *modereg = 0x40;
    tempa =  DTCLOWMASK(*Timer1);
    tempb =  DTCLOWMASK(*Timer1);
    value1.timer1 = DTCMASK(0x10000 - ((tempb << 8)|tempa));
    *modereg = 0x00;
    tempa =  DTCLOWMASK(*Timer0);
    tempb =  DTCLOWMASK(*Timer0);
    value1.timer0 = DTCMASK(0x10000 - ((tempb << 8)|tempa));

    /* Read (yes, a second read) the clock registers from the board */
    *modereg = 0x80;
    tempa =  DTCLOWMASK(*Timer2); 
    tempb =  DTCLOWMASK(*Timer2);
    value2.timer2 = DTCMASK(0x10000 - ((tempb << 8)|tempa));
    *modereg = 0x40;
    tempa =  DTCLOWMASK(*Timer1);
    tempb =  DTCLOWMASK(*Timer1);
    value2.timer1 = DTCMASK(0x10000 - ((tempb << 8)|tempa));
    *modereg = 0x00;
    tempa =  DTCLOWMASK(*Timer0);
    tempb =  DTCLOWMASK(*Timer0);
    value2.timer0 = DTCMASK(0x10000 - ((tempb << 8)|tempa));

    /* Check for a valid read */
    if ( (value1.timer2 == value2.timer2) &&
         (value1.timer1 == value2.timer1) &&
         (value1.timer0 <  value2.timer0) )
      correct_read = 1;
    counter++;
  }

  /* Use the value from the first read since that is the
   * one that we have guaranteed to be correct! */
  data.dtc_low = (value1.timer1 << 16) | value1.timer0;
  data.dtc_high = value1.timer2;

  /* Translate into sec and usec using dtcdiv() routine */
  dtcusec = data.dtc_low;
  dtcsec = data.dtc_high;
  dtcdiv();

  /* Add the dtctime to the basetime -- watching overflow in usec */
  time_value->tv_usec = basetime.tv_usec + dtcusec;
  if (time_value->tv_usec > 1000000)
  {
    dtcsec++;
    time_value->tv_usec = time_value->tv_usec - 1000000;
  }
  time_value->tv_sec = basetime.tv_sec + dtcsec;

  if (counter == MAXTRIES)
    return(-1);
  else
    return(0);
}
#endif
