#ifndef _EXTRA_STDLIB_H
#define _EXTRA_STDLIB_H

#include "i586-pc-cygwin32/include/stdlib.h"

struct qelem {
  struct qelem *q_forw;
  struct qelem *q_back;
  char q_data[1];
};

static inline void insque (struct qelem *__elem, struct qelem *__prev) 
{
  __elem->q_forw = __prev->q_forw;
  __elem->q_back = __prev;
  __prev->q_forw->q_back = __elem;
  __prev->q_forw = __elem;
}

static inline void remque (struct qelem *__elem)
{
  __elem->q_back->q_forw = __elem->q_forw;
  __elem->q_forw->q_back = __elem->q_back;
}

#endif






