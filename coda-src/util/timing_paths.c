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

static char *rcsid = "/afs/cs/project/coda-rvb/cvs/src/coda-4.0.1/coda-src/util/timing_paths.c,v 1.2 1997/01/07 18:41:49 rvb Exp";
#endif /*_BLURB_*/






#include <strings.h>
#include <sys/types.h>
#include <sys/file.h>
#ifdef __MACH__
#include <libc.h>
#endif /* __MACH__ */

#include <sys/time.h>
#include <math.h>
#include <dtcreg.h>
#include <histo.h>
#include "timing_paths.h"


#define PRIVATE static

#define DTCLOWMASK(n)	(n & 0xFF)
#define DTCMASK(n)	(n & 0xFFFF)


PRIVATE int bus_ptr,dcs_ptr;


ti_init()
    /* Initialises the timer */
    {
#ifdef ibm032
    open_timing_clock(&bus_ptr,&dcs_ptr);
    return(0);
#else
    printf("SORRY...TIMING NOT SUPPORTED\n");
    return(-1);
#endif ibm032
    }
    
ti_end()
    /* Releases the timer */
    {
#ifdef ibm032
    close_timing_clock(bus_ptr,dcs_ptr);
    return(0);
#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032
    
    }


ti_create(nEntries, thistie)
    int nEntries;
    struct tie *thistie;
    /* Creates a timer array of nEntries and initializes it.
       Returns 0 on success, -1 on failure.
    */
    {

    thistie->tiarray = (struct tientry*)malloc(nEntries*sizeof(struct tientry));

    if (thistie->tiarray == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}
    thistie->nEntries = nEntries;
    thistie->inuse = 0;
#ifdef ibm032
    if (readclock(&thistie->initcounters) < 0) return(-1);
    else return(0);
#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032

    }


ti_destroy(thistie)
    struct tie *thistie;
    /* Frees storage malloc'ed by ti_create();
       Returns 0.
    */
    {

    free(thistie->tiarray);
    thistie->nEntries = 0;
    return(0);
    }


ti_notetime(thistie, id)
    struct tie *thistie;
    long id;
    /* Notes current time in next slot of thistie.
       Returns 0 on success, -1 if thistie is full
    */

    {
    struct tientry *t;

    if (thistie->inuse >= thistie->nEntries) {
		printf("FULL!!\n");
		return(-1); /* full!! */}


    t = &thistie->tiarray[thistie->inuse];
    t->id = id;

#ifdef ibm032
    char *modereg = ModeReg;

/* Macro to read clock:
   * Modereg specifies the register to be read.
   * The first read of the Timer gets the low 8 bits and
   * the next read gets the high 8 bits.
   */
#define GETDT2806(i)\
   *modereg = 0x40;\
    t->dt2806[i] =  DTCLOWMASK(*Timer1);\
    t->dt2806[i+1] =  DTCLOWMASK(*Timer1);\
    *modereg = 0x00;\
    t->dt2806[i+2]  =  DTCLOWMASK(*Timer0);\
    t->dt2806[i+3]  =  DTCLOWMASK(*Timer0);

  /*  Read the clocks 3 times, to allow later checking of validity */

    GETDT2806(0);
    GETDT2806(4);
    GETDT2806(8);

#undef GETDT2806
    thistie->inuse++;
    return(0);

#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032

   }


ti_postprocess(thistie, twrt)
    struct tie *thistie;
    enum timewrt twrt;
    /*  First ensures than experiment took less than 35 minutes
	Processes times noted by ti_notetime(), and returns these
        times with respect to either first entry (twrt = BASELINE)
	or previous entry (twrt = DELTA)
	Returns: 0 if every entry successfully postprocessed
                -2 abandon run for one of the following:
		    if experiment took more than 35 minutes
		    very first entry bogus
    */
    {
    int i, rc =0 ;
    struct timeval  tempval;

#ifdef ibm032
    struct dtc_counters finalcount;
    char *modereg = ModeReg;
    int initvalue,endvalue;
    extern long dtcsec, dtcusec; /* defined in timing_clock.c;
                                globals for communicating with dtcdiv() */

    if (readclock(&finalcount) < 0) return(-1);


    /* Ensure that experinent did not take more than 35 minutes */
	
    dtcusec = ((thistie->initcounters.timer1 << 16) | thistie->initcounters.timer0 );

   dtcsec = thistie->initcounters.timer2;

   
  /* Translate into sec and usec using dtcdiv() routine */

  dtcdiv();

  /* Calculate time in minutes */
  initvalue = (dtcsec + dtcusec/1000000 )/60;

  dtcusec= ((finalcount.timer1 << 16) | finalcount.timer0 ); 
  dtcsec =  finalcount.timer2;

  /* Translate into sec and usec using dtcdiv() routine */

  dtcdiv();

  /* Calculate time in minutes */
  endvalue = (dtcsec + dtcusec/1000000 )/60;

   if (abs(endvalue - initvalue) >35)
	{/* more than one ripple in topmost counter */
	printf("ARGGGGGGH....TIMING RUN GREATER THAN 35 MINUTES!!\n");
	return(-2);
	}

    /* Obtain time for the first entry */
 
    if (thistie->tiarray[0].bogus)
	{
	printf("ARGGGH...INITIAL VALUE BOGUS!!\n");
	return(-2);
	}
#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032

    /* Note if time required is with respect to baseline or previous entry */

   thistie->timtype = twrt;
   switch (twrt)
       {
       case BASELINE:
           process_entries(thistie,&tempval);
           break;

       case DELTA :
           process_entries(thistie,&tempval);
 		struct timeval  tmp2, tmp1;
                tmp1.tv_sec= 0;
                tmp1.tv_usec=0;
           for ( i=1; i< thistie->inuse; i++)
              {

                struct tientry *t;
                t = &thistie->tiarray[i];
		tmp2 = t->tval;

 		if (t->id != 0)
                  {
                    if (t->bogus ==0)
                      {
	                t->tval.tv_usec = t->tval.tv_usec - tmp1.tv_usec;
	                t->tval.tv_sec = t->tval.tv_sec - tmp1.tv_sec;
                      }
            
                    else  while(t->id !=0)
                      {
                        t->bogus = 1;
                        i++;
                      }
                    tmp1 = tmp2;
		  }
		else { tmp1.tv_sec= 0; tmp1.tv_usec=0;}

	      }
	    break;

       }
    return (rc);

    }
 



ti_discoverpaths(thistie, pinfo)
    struct tie *thistie;
    struct pths_info *pinfo;
    {
    /* ti_discoverpaths walks through each of the entries in thistie 
       and identifies all the different paths taken by the program.
       For each path traversed, a structure is created which keeps
       track of the number of times each path was traversed,
       the starting indices of each of these paths and other relevant
       information. These structures are implemented as linked lists 
       pinfo points to first path.
    */

    int curr_ind = 0, r_cmp, i, rc=0;


    /* Create first path */
  
    pinfo->first_cand = (struct pth*) malloc(sizeof(struct pth));
    if (pinfo->first_cand == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}
    pinfo->first_cand->p_name = 0;

   /*  Initialise path */

    if(init_new_pth(thistie,pinfo->first_cand,curr_ind) != 0) { printf("Init Err\n");}
    curr_ind = curr_ind + calc_pth_lngth(thistie, curr_ind);
    pinfo->total_num_paths ++;



    /* Compare against existing path; if matches updates old path; else 
       create new path*/

         while( curr_ind<thistie->inuse)
       {
         r_cmp = compare_pth( thistie,pinfo, curr_ind);

         if (r_cmp== 0) 
           {
             if( create_new_pth(thistie, pinfo, curr_ind) !=0 ){ printf("Err create pth");}

             pinfo->total_num_paths ++;
           }
         curr_ind = curr_ind + calc_pth_lngth(thistie, curr_ind);


       }
      return(rc);

      }

    
ti_stat(thistie, pinfo)
    struct tie *thistie;
    struct pths_info *pinfo;
    /* ti_gen_stats generates the mean and standard deviation for 
       the id's followed by each path
    */

    {
    struct pth *y;
    struct p_ind *p;
    struct tientry *t;
    int *col,i,j, prb_num, rc=0;
    FILE *fp;
	 
    y = pinfo->first_cand;

    fp = fopen("histres","a");
    if (fp == (FILE*) NULL) printf("Cannot open\n");

    col = (int*) malloc( y->freq_occ*sizeof(int));	        
    if (col == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}

    for (i= 0; i< pinfo->total_num_paths; i++)
       {
	 fprintf(fp, "**********************************\n\n");
	 fprintf(fp, "THESE ARE THE RESULTS FOR PATH %d\n\n", y->p_name);

    /*Print id's of the corresponding path */
	 fprintf(fp, "THESE ARE THE ID'S :");

         for (j=0; j< y->p_length; j++)
            {
	      p =  y->start_ind;	      
              t= &thistie->tiarray[p->ind + j];

              fprintf(fp, " %d ", t->id);

	    }
         fprintf(fp, "\n\n");

 	 for ( prb_num =0; prb_num < y->p_length; prb_num++)
            {

	      gen_col( thistie, col, prb_num, y);

              call_histo( col, y->freq_occ, fp);

            }
         y = y->nxt_pth;
     
       }
       fclose (fp);
       return(rc);
    }



PRIVATE init_new_pth(thistie, pth_ptr, curr_add)
    struct tie *thistie;
    struct pth *pth_ptr;
    int curr_add;
    /* This routine initialises a new path, i.e. calculates the path length,
       sets the path freq count, sets the pointer to the struct which contains 
       the index of the path
    */

    {
     int rc = 0;
     struct p_ind *p;
     
     pth_ptr->p_length = calc_pth_lngth(thistie,curr_add);

     pth_ptr->freq_occ = 1;

     pth_ptr->start_ind = p= (struct p_ind*) malloc(sizeof(struct p_ind));
     if (p == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}
     pth_ptr->nxt_pth = (struct pth*) 0;
     p->ind = curr_add;

     p->nxt_ind = (struct p_ind*) 0;
     
     return (rc);
    }
  



PRIVATE compare_pth( thistie,pinfo, curr_add)
    struct tie *thistie;
    struct pths_info *pinfo;
    int curr_add;
    /* This routine compares two paths and calls the update_old_pth routine */

    {
    int rc, i;
    struct pth *y;
    struct p_ind *p;

    y = pinfo->first_cand;
    for (i=0; i< pinfo->total_num_paths; i++)
       {
	 p = y->start_ind;
         rc =  entry_cmp ( thistie, p->ind, curr_add, y->p_length);

         if ( rc == 1) 
           {
             if( update_old_pth( y, curr_add) !=0){ printf("Err Update\n");}
             break;
           }
         y = y->nxt_pth;
       }
    return(rc);
       
       }
    



PRIVATE update_old_pth( pth_ptr, curr_add)
    struct pth *pth_ptr;
    int curr_add;
    /* This routine updates the appropriate  existing path when a match occurs
       between the new path being compared with all the existing paths */

    {
    int rc =0;
    struct p_ind *p, *n;
   
    pth_ptr->freq_occ ++;

    p = pth_ptr->start_ind ;
 
    while ( p->nxt_ind != (struct p_ind*) 0 )
       {
         p = p->nxt_ind;
       }

    n = p->nxt_ind = (struct p_ind*) malloc(sizeof(struct p_ind));
    if (n == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}
    n->ind = curr_add;
    n->nxt_ind = (struct p_ind*) 0;
    return(rc);
   
    }



PRIVATE create_new_pth(thistie, pinfo, curr_add)
    struct tie *thistie;
    struct pths_info *pinfo;    
    int curr_add;
    {
    /* This routine creates a new path structure when no match is found
       between the existing paths and the path being compared */

    int rc = 0;
    struct pth *y, *t;

    y = pinfo->first_cand;

    while (y->nxt_pth != (struct pth*) 0 )
       {
         y = y->nxt_pth;
       }
     t = y->nxt_pth = (struct pth*) malloc(sizeof(struct pth));
    if (t == 0)
	{
	perror("Malloc failed.\n");
	return(-1);
	}
     t->p_name = y->p_name +1 ;

     init_new_pth(thistie, t, curr_add);
     return (rc);
     }



PRIVATE calc_pth_lngth(thistie, curr_add)
    struct tie *thistie;
    int curr_add;
    /* This routine calculates and returns the length of a path */

    {
    int count = 1;
    curr_add = curr_add +1;

    struct tientry *t;
    t = &thistie->tiarray[curr_add];


    while( t->id !=0)
      {
        count ++;
        curr_add ++;
        t = &thistie->tiarray[curr_add];
      }
        
    return(count);

    }





PRIVATE entry_cmp ( thistie, entry1, entry2, entry_count)
    int  entry1, entry2, entry_count;
    struct tie *thistie;
    /* Compares the id's of two sets of entries;
       Returns 1 on if same;
       Returns 0 if different;
    */
    {
    int k;

    for (k=0; k<entry_count+1; k++)
       {
         struct tientry *t,*f;
         t = &thistie->tiarray[entry1+k];
         f = &thistie->tiarray[entry2+k];

         if (t->id != f->id) return (0);
       }
     

    return(1);
    
    }



PRIVATE gen_col( thistie, col, prb_num, pth_ptr)
    struct tie *thistie;
    struct pth *pth_ptr;
    int prb_num, *col;
    /* This routine gets the data from thistie structure in the form required
       by the histogram routine */

    {
    int j;
    struct p_ind *p;
  
    p =  pth_ptr->start_ind;
    
    for ( j=0; j< pth_ptr->freq_occ; j++)
       {
          struct tientry *t;
 
          t= &thistie->tiarray[p->ind + prb_num];
          col[j] = (t->tval.tv_sec*1000000)+t->tval.tv_usec;
          p = p->nxt_ind;
       }
  
    }



PRIVATE call_histo( col_val,num_elm, fp)
    int *col_val;
    FILE *fp;
     /* This routine calls the histogram routine */
    { 

    double l_bound, h_bound;
    enum htype scale = LINEAR;
    int bk =10, j, rc;
    struct hgram Idhisto;


    /* First determine max and min times */

     l_bound = (double)min(col_val, num_elm) ;
     h_bound = (double)max(col_val, num_elm) +1.0;

     rc = InitHisto(&Idhisto, l_bound, h_bound, bk, scale);  
     if (rc < 0) printf("Initialisation failed\n");

     ClearHisto( &Idhisto);

     for (j=0;j< num_elm; j++)
        {

          UpdateHisto(&Idhisto,(double) col_val[j]);

        }



     PrintHisto(fp, &Idhisto);
     fprintf(fp, "---xxx---\n\n");
     }



PRIVATE int min(ar_ptr, num_elements)
    int *ar_ptr;
    int num_elements;
    /* This routine claculates the minimum in an array of numbers*/
    {
    int i,result;

    result = ar_ptr[0];
    for (i=0; i< num_elements; i++)
       {
   /*     printf("result for min %d \n", result);*/
         if (ar_ptr[i] < result) result = ar_ptr[i];
       }
    return(result);

    }




PRIVATE int max(ar_ptr, num_elements)
    int *ar_ptr;
    int num_elements;
    /* This routine calculates the max in an array of numbers */
    {
    int i, result;

    result = ar_ptr[0];
    for (i=0; i< num_elements; i++)
       {
          /*   printf("result for max %d \n", result);*/
         if (ar_ptr[i] > result) result = ar_ptr[i];
       }
    return(result);

    }

PRIVATE evaltime( i, thistie,tptr)
    int i;
    struct tie *thistie;
    struct timeval *tptr;
    /* Converts time in thistie->tiarray[i] into secs and usecs in *tptr;
       Returns 0 on success.
       Returns -1 and sets bogus field of entry, if two ripples in entry.
    */
    {
#ifdef ibm032
    int okread;
    struct dtc_counters reading[3];
    struct tientry *t;
    extern long dtcsec, dtcusec; /* defined in timing_clock.c;
                                globals for communicating with dtcdiv() */

    t = &thistie->tiarray[i];    


#define DTCOMPUTE(i) (DTCMASK(0x10000 - ((t->dt2806[i+1] << 8)|t->dt2806[i])))

    reading[0].timer1 = DTCOMPUTE(0);
    reading[0].timer0 = DTCOMPUTE(2);
    
    reading[1].timer1 = DTCOMPUTE(4);
    reading[1].timer0 = DTCOMPUTE(6);

    reading[2].timer1 = DTCOMPUTE(8);
    reading[2].timer0 = DTCOMPUTE(10);
#undef DTCOMPUTE

    /* Check for a valid read */
    if (reading[0].timer1 == reading[1].timer1) okread = 0;

    else
	{
	if (reading[1].timer1 == reading[2].timer1) okread = 1;

	else
	    {
	    /* Two consecutive reads with ripples! Can't handle */
	    t->bogus = 1;
	    return(-1);
	    }
	}
	


    /* We have a valid reading, and okread is it */
    t->bogus = 0;

    dtcusec = ((reading[okread].timer1 << 16) | reading[okread].timer0);
    if (reading[okread].timer1 < thistie->initcounters.timer1)
	{/* One ripple must have occured from middle to top counter */
	dtcsec = 1;
	}
    else dtcsec = 0;
    dtcdiv(); /* operands/results are in dtcsec and dtcusec */
    tptr->tv_sec = dtcsec;
    tptr->tv_usec = dtcusec;
	
    return(0);
#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032

    }


PRIVATE readclock(outval)
    struct dtc_counters *outval;  /* OUT: value of a good clock read */
    /* Returns 0 on success, -1 if no good reading on MAXTRIES */
    {
#ifdef ibm032
    int counter, correct_read;
    struct dtc_counters value1, value2;
    volatile long tempa, tempb;
    char *modereg = ModeReg;
#define MAXTRIES 10

    counter = 0;
    correct_read = 0;
    while ( (!correct_read) && (counter < MAXTRIES) )
    {

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

    if (counter >= MAXTRIES) return(-1);

  /* Use the value from the first read since that is the
   * one that we have guaranteed to be correct! */

    *outval = value1;

    return(0);
#else
    printf("TURKEY!!\n");
    return(-1);
#endif ibm032
    }


PRIVATE process_entries(thistie, tptr)
    struct tie *thistie;
    struct timeval *tptr;
    /* Processes each entry with respect to entry with id =0 */
    {
    int i, rc;
    struct timeval initval;

    /* Go through array and process each entry */
    rc = 0; /* tentatively */
    for (i=0 ; i< thistie->inuse; i++)
    {
    struct tientry *t;
    t = &thistie->tiarray[i];

    /* Evaluate time of each entry */

    evaltime(i, thistie, tptr);
   
   /* Obtain timing_intervals between each entry and first entry (id =0) */
   
    if (t->id == 0)
	{
   /* Set initial time value to that of id = 0  */

	 initval.tv_sec = tptr->tv_sec;
	 initval.tv_usec = tptr->tv_usec;
	}

    if (t->bogus == 0 )
	{
  /*  Compute time difference betwwen current entry and first entry */

	 tptr->tv_sec -= initval.tv_sec;	
	 tptr->tv_usec -= initval.tv_usec;

	if (tptr->tv_usec < 0 )
	   {
	   tptr->tv_sec -= 1;
	   tptr->tv_usec +=  1000000;
	   }

	t->tval.tv_sec = tptr->tv_sec;
	t->tval.tv_usec = tptr->tv_usec;

	}
    else 
	{
  /* Increment bogus count */

	thistie->num_bogus++;
	t->tval.tv_sec =  -1;
	t->tval.tv_usec = -1;
	}
    }

    return(rc);
    }

