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

enum VolumeStates { VSunknown, VSemulating, VShoarding, VSlogging, VSresolving };

class volent {
  friend int VolentPriorityFN(bsnode *, bsnode *);
  friend void PrintVDB(char *);
  friend StoplightStates StoplightState();

  char *name;
  VolumeId vid;
  VolumeStates state;

  bsnode queue_handle;		/* link for the volume queue */

 public:
    volent(char *Name, VolumeId id, VolumeStates theState);
    volent(volent&);
    int operator=(volent&);
    ~volent();
    int setState(VolumeStates newState);
    char *VolumeStateString();
    void print(FILE *f);
};
