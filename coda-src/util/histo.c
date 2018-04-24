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
                           none currently

#*/

#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include "coda_assert.h"
#include "histo.h"


int InitHisto(struct hgram *hg, double lolimit, double hilimit,
	      int bucketcount, enum htype ht)
    /* Initializes histogram hg. Returns 0 on success, -1 on failure.
       bucketcount is ignored if ht is LOG2 or LOG10  */
    {
    int i, t1, t2;

    if (lolimit >= hilimit) return(-1);
    if (ht != LINEAR && lolimit <= 0) return(-1);

    hg->type = ht;
    switch (ht)
	{
	case LINEAR:
	    hg->maxb = bucketcount;
	    hg->uflow.hival = lolimit;
	    hg->oflow.loval = hilimit; 
	    break;
	
	case LOG2:
	    t1 = (int) floor(log(lolimit)/LN2);
	    t2 = (int) ceil(log(hilimit)/LN2);
	    hg->maxb = t2 - t1;
	    hg->uflow.hival = RAISE2(t1);
	    hg->oflow.loval = RAISE2(t2);
	    break;
	
	case LOG10:
	    t1 = (int) floor(log10(lolimit));
	    t2 = (int) ceil(log10(hilimit));
	    hg->maxb = 9*(t2 - t1);
	    hg->uflow.hival = RAISE10(t1);
	    hg->oflow.loval = RAISE10(t2);
	    break;
	
	default:
	    return(-1);
	}
	
    
    hg->oflow.hival = HUGE_VAL;
    hg->uflow.loval = -HUGE_VAL;

    hg->buckets = (struct histo *) calloc(hg->maxb, sizeof(struct histo));
    if (!hg->buckets) return(-1); /* calloc() failed */
    
    /* NB: If you modify the code below make sure the change
       still avoids "gaps" in the histo buckets. Can be tricky.  */

    hg->buckets[0].loval = hg->uflow.hival;
    hg->buckets[hg->maxb-1].hival = hg->oflow.loval;
    
    if (ht == LINEAR)
	{
	double step;

	step = (hilimit - lolimit)/bucketcount;
	for (i = 0; i < hg->maxb-1; i++)
	    {
	    hg->buckets[i].hival = hg->buckets[i].loval + step;
	    hg->buckets[i+1].loval = hg->buckets[i].hival;
	    }
	}
    else
	{/* LOG2 or LOG10 */
	double adder;
	int logbase;
	
	logbase = (ht == LOG2 ? 2 : 10);
	adder = hg->buckets[0].loval;
	
	for (i = 0; i < hg->maxb-1; i++)
	    {
	    hg->buckets[i].hival = hg->buckets[i].loval + adder;
	    hg->buckets[i+1].loval = hg->buckets[i].hival;
	    if ((i+1) % (logbase - 1) == 0) adder *= logbase;
	    }
	}

    ClearHisto(hg);	
    
    return(0);
    }



void ClearHisto(struct hgram *hg)
    /* Clears histogram hg. */
    {
    int i;

    hg->count = 0;
    hg->sum = 0.0;
    hg->sum2 = 0.0;
    
    hg->oflow.count = 0;
    hg->uflow.count = 0;
    
    for (i = 0; i < hg->maxb; i++)
	hg->buckets[i].count = 0;
    }
    

void UpdateHisto(struct hgram *hg, double newval)
    /* hg -- histogram to be updated
       newval -- value to be entered */
    {
    MUpdateHisto(hg, newval, 1);
    }

void MUpdateHisto(struct hgram *hg, double newval, int number)
    /* hg -- histogram to be updated
       newval -- value to be entered 
       number -- number of newvals to be entered */
    {
    register int i;
    
    if (newval < hg->uflow.hival)
	{
	hg->uflow.count += number;
	return;
	}
	
    if (newval >= hg->oflow.loval)
	{
	hg->oflow.count += number;
	return;
	}

    for (i = 0; i < hg->maxb; i++)
	{/* I know I could use binary search here */
	if (newval >= hg->buckets[i].hival) continue;

	/* Foundit! */
	hg->buckets[i].count += number;
	hg->count += number;
	hg->sum += newval*(double)number;
	hg->sum2 += newval*newval*(double)number;
	CODA_ASSERT(hg->sum >= 0);
	CODA_ASSERT(hg->sum2 >= 0);
	return;
	}

    /* Should never get here */
    CODA_ASSERT(0);
    }


static double CIFactor(int dFreedom)
    /* dFreedom:  no of degrees of freedom; == (NoOfSamples - 1) */
    {
    /* Approximate; errs conservatively; see Law & Kelton, pg 386 */
    static double lowCI[20] = {-1.0, 3.078, 1.886, 1.638, 1.533, 1.476,
			1.440,1.415, 1.397, 1.383, 1.372,
			1.363, 1.356, 1.35, 1.345, 1.341, 1.337, 1.333, 1.33, 1.328};

    if (dFreedom >= 100) return(1.29);
    if (dFreedom >= 50)  return(1.3);
    if (dFreedom >= 30) return(1.31);
    if (dFreedom >= 20) return (1.325);
    return(lowCI[dFreedom]);
    }

int PrintHisto(FILE *outfile, struct hgram *hg)
    {
    double mean, stddev, c90;
    double temp;
    register int i;    
    
    /* Compute mean, std deviation and confidence interval */
    if (hg->count < 1) mean = 0.0;
    else  mean = hg->sum/(double)hg->count;

    if (hg->count < 2) stddev = c90 = 0.0;
    else
	{
	fprintf(outfile,"Count = %d, Sum = %g, Sum2 = %g\n", hg->count, hg->sum, hg->sum2); fflush(outfile);
	temp =	((double)hg->count*hg->sum2 - hg->sum*hg->sum) / ((double)hg->count*(double)(hg->count-1));
	CODA_ASSERT(temp >= 0);
	stddev = sqrt(temp);
	c90 = stddev*CIFactor(hg->count - 1);
	}
	
    if (fprintf(outfile, "Good samples:     count = %d    mean = %g   stddev = %g   90%%CI = %g\n\n",
    		hg->count, mean, stddev, c90) == -1) return -1;
    if (fprintf(outfile, "Bad samples:      underflow = %d    overflow = %d\n\n",
    		hg->uflow.count, hg->oflow.count) == -1) return -1;
    
    if (hg->uflow.count)
	if (fprintf (outfile, "        %d samples in the range -INFINITY to %g\n",
		hg->uflow.count, hg->uflow.hival) == -1) return -1;

    for (i = 0; i < hg->maxb; i++)
	{
	if (hg->buckets[i].count)
	    if (fprintf (outfile, "        %d samples in the range %g to %g\n",
		hg->buckets[i].count, hg->buckets[i].loval, hg->buckets[i].hival) == -1) return -1;
	}

    if (hg->oflow.count)
	if (fprintf (outfile, "        %d samples in the range %g to INFINITY\n",
	    	hg->oflow.count, hg->oflow.loval) == -1) return -1;
    return 0;
    }



int PlotHisto(FILE *outfile, struct hgram *hg, char *graphtitle, char *xtitle,
	      char *ytitle, char *psfileprefix)
     
    {
    register int i;
    int maxcount, totalcount;
    static int plotid = 1;

    if (fprintf(outfile, "New Page\n") == -1) return -1;
    if (fprintf(outfile, "New Graph\n") == -1) return -1;
    if (fprintf(outfile, "Graph Title %s\n", graphtitle) == -1)
      return -1;
    if (fprintf(outfile, "Curve Type Solid\n") == -1) return -1;
    if (fprintf(outfile, "Curve Symbol None\n") == -1) return -1;
    if (fprintf(outfile, "Curve Interpolation XHistogram\n") == -1)
      return -1;
    if (fprintf(outfile, "Curve Confidence no\n") == -1) return -1;

    /* Obtain total count to estimate percentages */
    totalcount = maxcount = 0;
    for (i = 0; i < hg->maxb; i++)
	{
	totalcount += hg->buckets[i].count;
	if (hg->buckets[i].count > maxcount) maxcount = hg->buckets[i].count;
	}

    /* Print out the points */
    if (fprintf(outfile, "New Points\n") == -1) return -1;
    for (i = 0; i < hg->maxb; i++)
	{
	if (fprintf(outfile, "%g %g\n", hg->buckets[i].loval, (hg->buckets[i].count*100.0)/(totalcount*1.0)) == -1)
            return -1;
	}
    if (fprintf(outfile, "\n") == -1) return -1;
	
    if (fprintf(outfile, "X Minimum %g\n", hg->uflow.hival) == -1)
      return -1;
    if (fprintf(outfile, "X Maximum %g\n", hg->oflow.loval) == -1) 
      return -1;
    switch(hg->type)
	{
	case LINEAR:
	    if (fprintf(outfile, "X Scale Linear\n") == -1)
              return -1;
	    break;
	
	case LOG2:
	    if (fprintf(outfile, "X LogBase 2\n") == -1) return -1;
	    if (fprintf(outfile, "X Scale Log\n") == -1) return -1;
	    break;	
	
	case LOG10:
	    if (fprintf(outfile, "X LogBase 10\n") == -1) 
              return -1;
	    if (fprintf(outfile, "X Scale Log\n") == -1) return -1;
	    break;	

	}
    if (fprintf(outfile, "X Label %s\n", xtitle) == -1) return -1;

    if (fprintf(outfile, "Y Minimum %d\n", 0) == -1) return -1;
    if (fprintf(outfile, "Y Maximum %g\n", ceil((maxcount*100.0)/(totalcount*1.0)) ) == -1) return -1;
    if (fprintf(outfile, "Y Scale Linear\n") == -1) return -1;
    if (fprintf(outfile, "Y Label %s\n", ytitle) == -1) return -1;


    if (fprintf(outfile, "Plot postscript %s%d.PS nodocument noprint\n\n", psfileprefix, plotid++) == -1) 
              return -1;
    return 0;
    }
