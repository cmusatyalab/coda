
#include <util.h>
#include <../partition.h>
#include <../vicetab.h>
extern void  printnames(struct DiskPartition *dp, int low, int step, int high);

int
main(int argc, char **argv)
{
    struct DiskPartition *dp;
    Inode testcreate;
    Device devno;
    int fd, count;
    char *buff="This is a test string";
    Partent pe, pf;
    FILE *vtab;
    int rc;
    char host[256];

    /* write a vicetab file */
    hostname(host);

    /* set up a simple partition & ftree partition */
    /* they must be on different diskpartions */
    pe = Partent_create(host, "simpled", "simple","");
    pf = Partent_create(host, "/tmp/f", "ftree","width=8,depth=5");
    unlink("vicetab");
    rc = creat("vicetab", 00600);
    assert( rc != -1 );
    vtab = Partent_set("vicetab", "r+");
    rc = Partent_add(vtab, pe);
    assert( rc == 0 );
    rc = Partent_add(vtab, pf);
    assert( rc == 0 );
    Partent_free(&pe);
    Partent_free(&pf);
    Partent_end(vtab);
    printf("Make sure to run makeftree vicetab /tmp/f before continuing!\n");
    return 0;
    
}
