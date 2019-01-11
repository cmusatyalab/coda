#include "pdbarray.h"

void print_array(pdb_array *a)
{
    int i, n = pdb_array_size(a);
    printf("[ ");
    for (i = 0; i < n; i++)
        printf("%d ", a->data[i]);
    printf("]\n");
}

int main(int argc, char *argv[])
{
    pdb_array a, b;

    printf("Adding 2, 4, expecting [ 2 4 ]\n");
    pdb_array_init(&a);
    pdb_array_add(&a, 2);
    pdb_array_add(&a, 4);
    print_array(&a);

    printf("Adding 5, 3, 1, expecting [ 1 2 3 4 5 ]\n");
    pdb_array_add(&a, 5);
    pdb_array_add(&a, 3);
    pdb_array_add(&a, 1);
    print_array(&a);

    printf("Removing 5, 1, 3, expecting [ 2 4 ]\n");
    pdb_array_del(&a, 5);
    pdb_array_del(&a, 1);
    pdb_array_del(&a, 3);
    print_array(&a);

    printf("Merging with array b [ 1 3 5 ], expecting [ 1 2 3 4 5 ]\n");
    pdb_array_init(&b);
    pdb_array_add(&b, 1);
    pdb_array_add(&b, 3);
    pdb_array_add(&b, 5);
    pdb_array_merge(&a, &b);
    pdb_array_free(&b);
    print_array(&a);

    pdb_array_free(&a);
}
