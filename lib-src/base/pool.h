
#include "bitvect.h"

typedef struct Pool *PPool;

void P_Destroy(PPool *pool);
PPool P_New(int count, int size);
void *P_Malloc(PPool);
void P_Free(PPool, void *);
