
#ifndef _TALLYENT_H_
#define _TALLYENT_H_ 1

#include <dlist.h>

enum TallyStatus { TSavailable, TSunavailable, TSunknown };


class tallyent {
  friend void InitTally();
  friend int tallyentPriorityFN(dlink *, dlink *);
  friend void Tally(int, vuid_t, int, TallyStatus);
  friend dlink *Find(int, vuid_t);
  friend void TallyPrint(vuid_t);
  friend void TallySum(int *, int *);
  friend void NotifyUsersTaskAvailability();

  dlink prioq_handle;

  int priority;
  vuid_t vuid;
  int available_blocks;
  int available_files;
  int unavailable_blocks;
  int unavailable_files;
  int incomplete;  

 public:
   tallyent(int priority, vuid_t vuid, int blocks, TallyStatus status);
   tallyent(tallyent&);
   operator=(tallyent&);
   ~tallyent();
};

extern dlist *TallyList;

extern void InitTally();
extern dlink *Find(int priority, vuid_t uid);
extern void Tally(int, vuid_t, int, TallyStatus);
extern void TallyPrint(vuid_t);
extern void TallySum(int *, int *);


#endif not _TALLYENT_H_
