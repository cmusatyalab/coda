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

class miss {
  friend int PathnamePriorityFN(bsnode *, bsnode *);
  friend void PrintMissList(char *);
  friend void ReinstatePreviousMissQueue();
    char *path;
    char *program;
    int num_instances;

    bsnode queue_handle;                 /* link for the cache miss queues */

  public:
    miss(char *Path, char *Program);
    miss(miss&);                         /* not supported! */
    int operator=(miss&);                    /* not supported! */
    ~miss();
    void print(FILE *f);
};

extern void InitMissQueue();
extern void OutputMissStatistics();

#define TMPMISSLIST "/tmp/misslist"

