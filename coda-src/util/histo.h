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

#ifndef _HISTO_
#define _HISTO_

/* Histogram scaling */
enum htype
{
    LINEAR = 1,
    LOG2   = 2,
    LOG10  = 3
};
#define LN2 0.69315 /* natural logarithm of 2.0 */
#define RAISE2(x) (pow(2.0, 1.0 * (x)))
#define RAISE10(x) (pow(10.0, 1.0 * (x)))

/* One entry in a histogram. */
struct histo {
    /* I know it's not the most efficient in storage.
       hival and loval are not both necessary.
       But it makes life so much simpler!
    */
    double loval; /* matching values are >= loval */
    double hival; /* matching values are < hival */
    int count; /* number of matching values */
};

/* An entire histogram */
struct hgram {
    int maxb; /* number of buckets */
    enum htype type; /* what kind of histogram */
    struct histo *buckets; /* malloc'ed array of maxb buckets */
    struct histo oflow; /* values > buckets[maxb-1].hival */
    struct histo uflow; /* values < buckets[0].loval */
    int count; /* total no of entries not in oflow or uflow */
    double sum; /* sum of values (not in oflow or uflow) */
    double sum2; /* sum of squares of values (not in oflow or uflow) */
};

extern int InitHisto(struct hgram *, double, double, int, enum htype);
extern void ClearHisto(struct hgram *);
extern void UpdateHisto(struct hgram *, double);
extern void MUpdateHisto(struct hgram *, double, int);
extern int PrintHisto(FILE *, struct hgram *);
extern int PlotHisto(FILE *, struct hgram *, char *, char *, char *, char *);
#endif /* _HISTO_ */
