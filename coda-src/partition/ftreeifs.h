#include <lock.h>
#include <bitvect.h>

#define RESOURCEDB "FTREEDB"
struct part_ftree_opts {
    int depth;
    int width;
    int logwidth;
    int resource;
    int next;
    Bitv freebm;
    Lock lock;
};
