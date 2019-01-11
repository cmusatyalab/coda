/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
*
*                   RVM internal structure debugging functions
*
*/

#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include "rvm_private.h"

/* globals */

extern rvm_length_t page_size;
extern rvm_length_t page_mask;
extern rvm_bool_t rvm_no_log;
extern char *rvm_errmsg; /* internal error message buffer */

/* roots of memory structures */

/* structure cache */
extern long type_counts[NUM_CACHE_TYPES];
extern list_entry_t free_lists[NUM_CACHE_TYPES];
extern long pre_alloc[NUM_CACHE_TYPES];
extern long max_alloc[NUM_CACHE_TYPES];
extern long cache_type_sizes[NUM_CACHE_TYPES];

/* main structures roots */
extern list_entry_t seg_root; /* segment list */
extern tree_node_t *region_tree; /* mapped regions tree */
extern list_entry_t page_list; /* free page list */
extern list_entry_t log_root; /* log list */

/* locals */

/* structure names & sizes for debug support */
#ifdef DEBUG_GDB
static char *type_names[NUM_TYPES]        = { TYPE_NAMES };
static rvm_length_t type_sizes[NUM_TYPES] = { CACHE_TYPE_SIZES,
                                              OTHER_TYPE_SIZES };
#define SIZE(id) (type_sizes[ID_INDEX(id)])
#define NAME(id) (type_names[ID_INDEX(id)])

/* address is in a structure */
#define IN_STRUCT(x, s, id) \
    (((x) >= (rvm_length_t)(s)) && ((x) < (((rvm_length_t)(s)) + SIZE(id))))

/* address validation: must be on rvm_length_t size boundary */
#define ADDR_INVALID(x) ((rvm_length_t)(x) != CHOP_TO_LENGTH((x)))

#define ADDR_INVALID_OR_NULL(x) (ADDR_INVALID(x) || ((x) == NULL))
#endif /* DEBUG_GDB */

/* empty routine to force loading of this module when referenced by a program
   can also be used as a break point when a condition must be calculated */
void rvm_debug(val) rvm_length_t val;
{
    if (val != 0)
        printf("\nAt rvm_debug: %ld (%lx)\n", val, val);
}

#ifdef DEBUG_GDB
/* power of 2 table -- must be extended for machines with address
     spaces greater than 32 bits */
#define NUM_TWOS 30
static rvm_length_t twos[NUM_TWOS] = {
    1 << 3,  1 << 4,  1 << 5,  1 << 6,  1 << 7,  1 << 8,  1 << 9,  1 << 10,
    1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16, 1 << 17, 1 << 18,
    1 << 19, 1 << 20, 1 << 21, 1 << 22, 1 << 23, 1 << 24, 1 << 25, 1 << 26,
    1 << 27, 1 << 28, 1 << 29, 1 << 30, 1 << 31, -1
};
/* test ifaddress is in heap-allocated space */
rvm_bool_t in_heap(addr, buf,
                   len) rvm_length_t addr; /* address to search for */
rvm_length_t buf; /* buffer to search */
rvm_length_t len; /* requested length of buffer */
{
    long i;

    if (buf == 0)
        return rvm_false; /* skip null buffers */

    len += sizeof(rvm_length_t); /* compensate for malloc back ptr */
    buf -= sizeof(rvm_length_t);
    for (i = 0; i < NUM_TWOS; i++)
        if ((len >= twos[i]) && (len < twos[i + 1]))
            break;
    assert(i != NUM_TWOS);

    if ((addr >= buf) && (addr < (buf + twos[i])))
        return rvm_true;

    return rvm_false;
}
/* list checker -- makes sure elements of list are valid */
rvm_bool_t chk_list(hdr,
                    silent) list_entry_t *hdr; /* header of list to check */
rvm_bool_t silent; /* print only errors if true */
{
    list_entry_t *entry; /* current list entry */
    list_entry_t *prev; /* previous list entry */
    long i            = 0;
    rvm_bool_t retval = rvm_true;

    if (hdr == NULL) {
        printf("  List header is null\n");
        return rvm_false;
    }
    if (ADDR_INVALID(hdr)) {
        printf("  List header address invalid, hdr = %lx\n", (long)hdr);
        return rvm_false;
    }
    if (hdr->is_hdr != rvm_true) {
        printf("  List header is not valid, is_hdr = %ld\n", (long)hdr->is_hdr);
        return rvm_false;
    }
    if (!(((long)hdr->struct_id > (long)struct_first_id) &&
          ((long)hdr->struct_id < (long)struct_last_id))) {
        printf("  List header type is not valid, struct_id = %ld\n",
               (long)hdr->struct_id);
        return rvm_false;
    }
    if (hdr->list.length < 0)
        printf("  List length invalid, length = %ld\n", hdr->list.length);
    if (ADDR_INVALID_OR_NULL(hdr->nextentry)) {
        printf("  List header at %lx has invalid nextentry field, ", (long)hdr);
        printf("hdr->nextentry = %lx\n", (long)hdr->nextentry);
        return rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(hdr->preventry)) {
        printf("  List header at %lx has invalid preventry field, ", (long)hdr);
        printf("hdr->preventry = %lx\n", (long)hdr->nextentry);
        return rvm_false;
    }
    if ((hdr->nextentry == hdr->preventry) && (hdr->nextentry == hdr)) {
        if (!silent)
            printf("  List empty\n");
        if (hdr->list.length != 0) {
            printf("  List length invalid, length = %ld\n", hdr->list.length);
            return rvm_false;
        }
        return rvm_true;
    }
    if (!silent)
        printf("  List length = %ld\n", hdr->list.length);
    /* check ptrs */
    if (ADDR_INVALID_OR_NULL(hdr->nextentry)) {
        printf("  List header at %lx has invalid nextentry field, ", (long)hdr);
        printf("hdr->nextentry = %lx\n", (long)hdr->nextentry);
        return rvm_false;
    }

    /* check all elements of list */
    prev  = hdr;
    entry = hdr->nextentry;
    while (entry->is_hdr != rvm_true) {
        i++;
        if (hdr->struct_id != entry->struct_id) {
            printf("  List entry %ld (%lx) has wrong type, struct_id = %ld, ",
                   i, (long)entry, (long)entry->struct_id);
            printf("hdr->struct_iud = %ld\n", (long)hdr->struct_id);
            retval = rvm_false;
        }
        if (entry->list.name != hdr) {
            printf(
                "  List entry %ld (%lx) does not point to header, name = %lx\n",
                i, (long)entry, (long)entry->list.name);
            retval = rvm_false;
        }
        if (entry->preventry != prev) {
            printf("  List entry %ld (%lx)does not have correct preventry,", i,
                   (long)entry);
            printf(" preventry = %lx\n", (long)entry->preventry);
            retval = rvm_false;
        }
        if (ADDR_INVALID_OR_NULL(entry->nextentry)) {
            printf("  List entry %ld (%lx) has invalid nextentry field, ", i,
                   (long)entry);
            printf("nextentry = %lx\n", (long)entry->nextentry);
            return rvm_false;
        }
        prev  = entry;
        entry = entry->nextentry;
    }
    /* check results */
    if (i != hdr->list.length) {
        printf("  List length wrong, length = %ld, actual length = %ld\n",
               hdr->list.length, i);
        retval = rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(hdr->preventry)) {
        printf("  List header at %lx has invalid preventry field, ", (long)hdr);
        printf("hdr->preventry = %lx\n", (long)hdr->nextentry);
        retval = rvm_false;
    }

    if ((retval) && (!silent))
        printf("  List is OK\n");

    return retval;
}
/* structure cache free list checker */
rvm_bool_t chk_free_list(struct_id)
    struct_id_t struct_id; /* type of free list to check */
{
    if (!(((long)struct_id > (long)struct_first_id) &&
          ((long)struct_id < (long)struct_last_cache_id))) {
        printf("This structure is not cached\n");
        return rvm_false;
    }

    return chk_list(&free_lists[ID_INDEX(struct_id)], rvm_true);
}

/* check all free lists */
void chk_all_free_lists()
{
    long i;

    for (i = ID_INDEX(log_id); i < ID_INDEX(struct_last_cache_id); i++) {
        printf("Checking free list for %s\n", type_names[i]);
        chk_free_list(INDEX_ID(i));
    }
}
/* locate an address in simple list */
rvm_bool_t search_list(hdr, struct_id,
                       addr) list_entry_t *hdr; /* header of list to search */
struct_id_t struct_id; /* type of list to search */
rvm_length_t addr; /* address to search for */
{
    list_entry_t *entry; /* current list entry */
    long i            = 0;
    rvm_bool_t pr_hdr = rvm_true;
    rvm_bool_t retval = rvm_false;

    /* see if in header */
    if (hdr == NULL)
        return rvm_false;
    if ((addr >= (rvm_length_t)hdr) && (addr < ((rvm_length_t)hdr + addr))) {
        printf("  Address contained in %s list header at %lx\n",
               NAME(struct_id), (long)hdr);
        retval = rvm_true;
    }

    /* search the list */
    entry = hdr->nextentry;
    while (!entry->is_hdr) {
        i++;
        if ((addr >= (rvm_length_t)entry) &&
            (addr < ((rvm_length_t)entry + SIZE(struct_id)))) {
            if (pr_hdr) {
                printf("  Address contained in %s list at %lx\n",
                       NAME(struct_id), (long)hdr);
                pr_hdr = rvm_false;
            }
            printf("   in entry %ld at %lx\n", i, (long)entry);
            retval = rvm_true;
        }
        entry = entry->nextentry;
    }
    return retval;
}
/* locate an address in free page list */
rvm_bool_t
    in_free_page_list(addr) rvm_length_t addr; /* address to search for */
{
    free_page_t *pg;
    rvm_bool_t retval = rvm_false;

    /* sanity check the list structure */
    printf("Searching free page list\n");
    if (!chk_list(&page_list, rvm_true))
        return rvm_false;

    /* search the pages */
    FOR_ENTRIES_OF(page_list, free_page_t, pg)
    {
        if ((addr >= (rvm_length_t)pg) &&
            (addr < ((rvm_length_t)pg + pg->len))) {
            printf("  Address contained in free page entry at %lx\n", (long)pg);
            retval = rvm_true;
        }
    }

    return retval;
}
/* locate an address in free list */
rvm_bool_t in_free_list(struct_id, addr)
    struct_id_t struct_id; /* type of free list to search */
rvm_length_t addr; /* address to search for */
{
    /* check basic list structure */
    if (!chk_list(&free_lists[ID_INDEX(struct_id)], rvm_true))
        return rvm_false;

    /* see if addr is in any element of list */
    return search_list(&free_lists[ID_INDEX(struct_id)], struct_id, addr);
}

/* locate an address in free lists (searches all) */
rvm_bool_t in_free_lists(addr) rvm_length_t addr; /* address to search for */
{
    rvm_bool_t retval = rvm_false;
    long i;

    for (i = ID_INDEX(log_id); i < ID_INDEX(struct_last_cache_id); i++) {
        printf("Searching free list %s\n", type_names[i]);
        if (in_free_list(INDEX_ID(i), addr))
            retval = rvm_true;
    }

    return retval;
}
/* mem_region_t tree node checks */
rvm_bool_t chk_mem_node(node) mem_region_t *node;
{
    region_t *region;
    seg_t *seg;
    rvm_bool_t retval = rvm_true;

    /* basic memory region node checks */
    if (ADDR_INVALID_OR_NULL(node->region)) {
        printf("  Region ptr is invalid, node->object = %lx\n",
               (long)node->region);
        return rvm_false;
    }
    region = node->region;
    if (region->links.struct_id != region_id) {
        printf("  Mem_region node at %lx does not point to", (long)node);
        printf(" region descriptor\n");
        return rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(region->mem_region) ||
        ((mem_region_t *)region->mem_region != node)) {
        printf("  Region descriptor at %lx does not point back to",
               (long)region);
        printf(" mem_region node at %lx\n", (long)node);
        return rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(region->seg)) {
        printf("  Mem_region node at %lx region descriptor has invalid",
               (long)node);
        printf(" segment ptr, ptr = %lx\n", (long)region->seg);
        return rvm_false;
    }
    if (region->seg->links.struct_id != seg_id) {
        printf("  Mem_region node at %lx region descriptor has invalid",
               (long)node);
        printf(" segment descriptor, seg = %lx\n", (long)region->seg);
        return rvm_false;
    }
    /* related structure and list checks */
    if (!chk_list(&seg_root, rvm_true))
        return rvm_false;
    FOR_ENTRIES_OF(seg_root, seg_t, seg)
    if (seg == region->seg)
        break;
    if ((list_entry_t *)seg == &seg_root) {
        printf("  Mem_region node at %lx region descriptor's segment",
               (long)region);
        printf(" descriptor is not on seg_root list, seg = %lx\n",
               (long)region->seg);
        retval = rvm_false;
    }

    seg = region->seg;
    if (!chk_list(&seg->map_list, rvm_true)) {
        printf("  Mem_region's region's segment's map_list is damaged,");
        printf(" seg = %lx\n", (long)seg);
        return rvm_false;
    }
    FOR_ENTRIES_OF(seg->map_list, region_t, region)
    if (region == node->region)
        break;
    if (region != node->region) {
        printf("  Mem_region node at %lx region descriptor is", (long)node);
        printf(" not on its segment's map_list, region = %lx\n",
               (long)node->region);
        return rvm_false;
    }
    region = node->region;
    if (region->links.struct_id != region_id) {
        printf("  Mem_region node at %lx does not point to", (long)node);
        printf(" region descriptor\n");
        return rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(region->mem_region) ||
        ((mem_region_t *)region->mem_region != node)) {
        printf("  Region descriptor at %lx does not point back to",
               (long)region);
        printf(" mem_region node at %lx\n", (long)node);
        return rvm_false;
    }
    if (ADDR_INVALID_OR_NULL(region->seg)) {
        printf("  Mem_region node at %lx region descriptor has invalid",
               (long)node);
        printf(" segment ptr, ptr = %lx\n", (long)region->seg);
        return rvm_false;
    }
    if (region->seg->links.struct_id != seg_id) {
        printf("  Mem_region node at %lx region descriptor has invalid",
               (long)node);
        printf(" segment descriptor, seg = %lx\n", (long)region->seg);
        return rvm_false;
    }
    /* related structure and list checks */
    if (!chk_list(&seg_root, rvm_true))
        return rvm_false;
    FOR_ENTRIES_OF(seg_root, seg_t, seg)
    if (seg == region->seg)
        break;
    if ((list_entry_t *)seg == &seg_root) {
        printf("  Mem_region node at %lx region descriptor's segment",
               (long)region);
        printf(" descriptor is not on seg_root list, seg = %lx\n",
               (long)region->seg);
        retval = rvm_false;
    }

    seg = region->seg;
    if (!chk_list(&seg->map_list, rvm_true)) {
        printf("  Mem_region's region's segment's map_list is damaged,");
        printf(" seg = %lx\n", (long)seg);
        return rvm_false;
    }
    FOR_ENTRIES_OF(seg->map_list, region_t, region)
    if (region == node->region)
        break;
    if (region != node->region) {
        printf("  Mem_region node at %lx region descriptor is", (long)node);
        printf(" not on its segment's map_list, region = %lx\n",
               (long)node->region);
        retval = rvm_false;
    }

    if (!chk_list(&seg->unmap_list, rvm_true)) {
        printf("  Mem_region's region's segment's unmap_list is damaged,");
        printf(" seg = %lx\n", (long)seg);
        return rvm_false;
    }
    FOR_ENTRIES_OF(seg->unmap_list, region_t, region)
    if (region == node->region) {
        printf("  Mem_region node at %lx region descriptor is", (long)node);
        printf(" on its segment's unmap_list, region = %lx\n", (long)region);
        retval = rvm_false;
        break;
    }

    return retval;
}
/* validate dev_region node */
rvm_bool_t chk_dev_node(dev_region_t *node)
{
    rvm_bool_t retval = rvm_true;
    /* check validity of nv buffer ptrs */
    if (!((node->nv_ptr == NULL) && (node->nv_buf == NULL))) {
        if (ADDR_INVALID_OR_NULL(node->nv_ptr)) {
            printf("  Dev_region node at %lx has bad nv_ptr\n", (long)node);
            retval = rvm_false;
        }
        if (ADDR_INVALID(node->nv_buf)) {
            printf("  Dev_region node at %lx has bad nv_buf\n", (long)node);
            retval = rvm_false;
        }
    }

    /* check consistency of nv buffer vs. log offset */
    if (!((node->nv_ptr != NULL) && RVM_OFFSET_EQL_ZERO(node->log_offset)) ||
        ((!RVM_OFFSET_EQL_ZERO(node->log_offset)) && (node->nv_ptr == NULL))) {
        printf("  Dev_region node at %lx has inconsistent nv_ptr", (long)node);
        printf(" & log_offset\n");
        retval = rvm_false;
    }

    return retval;
}
/* check validity of tree node */
rvm_bool_t chk_node(tree_node_t *node, struct_id_t struct_id)
{
    rvm_bool_t retval = rvm_true;

    /* basic structure checks */
    if (node->struct_id != struct_id) {
        printf("  Node at %lx has wrong struct_id, id = %d, should be %ld\'n",
               (long)node, node->struct_id, (long)struct_id);
        retval = rvm_false;
    }
    if (node->gtr != NULL)
        if (ADDR_INVALID(node->gtr) || (node->gtr->struct_id != struct_id)) {
            printf("  Node at %lx gtr ptr invalid\n", (long)node);
            retval = rvm_false;
        }
    if (node->lss != NULL)
        if (ADDR_INVALID(node->lss) || (node->lss->struct_id != struct_id)) {
            printf("  Node at %lx lss ptr invalid\n", (long)node);
            retval = rvm_false;
        }

    /* type-specific checks */
    switch (struct_id) {
    case mem_region_id:
        retval = chk_mem_node((mem_region_t *)node) && retval;
        break;
    case dev_region_id:
        retval = chk_dev_node((dev_region_t *)node) && retval;
        break;
    default:
        assert(rvm_false);
    }

    return retval;
}
/* search mem_region tree node */
rvm_bool_t
    search_mem_region(addr, node) rvm_length_t addr; /* address to search for */
mem_region_t *node; /* mem_region node to search */
{
    rvm_bool_t retval = rvm_false;

    /* check tree node */
    if (!chk_node((tree_node_t *)node, mem_region_id))
        return rvm_false;

    /* see if address is in node */
    if ((addr >= (rvm_length_t)node) &&
        (addr < ((rvm_length_t)node + SIZE(mem_region_id)))) {
        printf("  ***  Address is in mem_region node at %lx\n", (long)node);
        retval = rvm_true;
    }

    /* see if address is in node's vm */
    if ((addr >= (rvm_length_t)node->vmaddr) &&
        (addr < ((rvm_length_t)node->vmaddr + node->length))) {
        printf(
            "  ***  Address is in vm represented by mem_region node at %lx\n",
            (long)node);
        retval = rvm_true;
    }

    /* check lower branches */
    if (node->links.node.lss != NULL)
        if (search_mem_region(addr, (mem_region_t *)node->links.node.lss))
            retval = rvm_true;
    if (node->links.node.gtr != NULL)
        if (search_mem_region(addr, (mem_region_t *)node->links.node.gtr))
            retval = rvm_true;

    return retval;
}

/* locate an address in region_tree */
rvm_bool_t in_region_tree(addr) rvm_length_t addr; /* address to search for */
{
    printf("Searching mapped region tree\n");

    return search_mem_region(addr, (mem_region_t *)region_tree);
}
/* */
rvm_bool_t
    search_dev_region(addr, node) rvm_length_t addr; /* address to search for */
dev_region_t *node; /* segment region node to search */
{
    rvm_bool_t retval = rvm_false;

    /* basic node checks */
    if (!chk_node((tree_node_t *)node, dev_region_id))
        return rvm_false;

    /* see if addr is in node */
    if (IN_STRUCT(addr, node, dev_region_id)) {
        printf("  ***  Address is in dev_region node at %lx\n", (long)node);
        retval = rvm_true;
    }

    /* see if address is in node's nv buffer */
    if (node->nv_ptr != NULL)
        if (in_heap(addr, (rvm_length_t)node->nv_buf,
                    node->nv_buf->alloc_len)) {
            printf("  ***  Address is in dev_region at %lx nv buffer\n",
                   (long)node);
            retval = rvm_true;
        }

    /* check lower nodes */
    if (node->links.node.lss != NULL)
        if (search_dev_region(addr, (dev_region_t *)node->links.node.lss))
            retval = rvm_true;
    if (node->links.node.gtr != NULL)
        if (search_dev_region(addr, (dev_region_t *)node->links.node.gtr))
            retval = rvm_true;

    return retval;
}
/* search region descriptor */
rvm_bool_t in_region(addr, region,
                     n) rvm_length_t addr; /* address to search for */
region_t *region; /* region descriptor to search */
long n;
{
    rvm_bool_t retval = rvm_false;

    /* see if in region descriptor */
    printf("    Searching region %ld\n", n);
    if (IN_STRUCT(addr, region, region_id)) {
        printf("  ***  Address is in region descriptor at %lx\n", (long)region);
        retval = rvm_true;
    }

    return retval;
}
/* search segment descriptor */
rvm_bool_t in_seg(addr, seg, n) rvm_length_t addr; /* address to search for */
seg_t *seg; /* segment descriptor to search */
long n;
{
    region_t *region, *region2;
    long i            = 0;
    rvm_bool_t retval = rvm_false;

    /* see if address is in descriptor */
    printf("  Searching segment %ld\n", n);
    if (IN_STRUCT(addr, seg, seg_id)) {
        printf("  ***  Address is in segment descriptor at %lx\n", (long)seg);
        retval = rvm_true;
    }

    /* see if in device name */
    if (ADDR_INVALID_OR_NULL(seg->dev.name))
        printf("  Segment descriptor at %lx has bad dev.name\n", (long)seg);
    else if (in_heap(addr, (rvm_length_t)seg->dev.name, seg->dev.name_len)) {
        printf("  ***  Address is in segment at %lx device name\n", (long)seg);
        retval = rvm_true;
    }

    /* validate and scan mapping lists */
    if (!chk_list(&seg->map_list, rvm_true)) {
        printf("  Segment descriptor at %lx has bad map list\n", (long)seg);
        return retval;
    }
    if (!chk_list(&seg->unmap_list, rvm_true)) {
        printf("  Segment descriptor at %lx has bad unmap list\n", (long)seg);
        return retval;
    }
    FOR_ENTRIES_OF(seg->map_list, region_t, region)
    {
        i++;
        if (in_region(addr, region, i)) {
            printf("  ***  Address is in region descriptor at %lx\n",
                   (long)region);
            retval = rvm_true;
        }
        FOR_ENTRIES_OF(seg->unmap_list, region_t, region2)
        if (region == region2) {
            printf("  Region descriptor at %lx is on both map and unmap",
                   (long)region);
            printf(" lists of segment descriptor at %lx\n", (long)seg);
            break;
        }
    }
    i = 0;
    FOR_ENTRIES_OF(seg->unmap_list, region_t, region)
    {
        i++;
        if (in_region(addr, region, i)) {
            printf("  ***  Address is in region descriptor at %lx\n",
                   (long)region);
            retval = rvm_true;
        }
    }

    return retval;
}
/* search segment list */
rvm_bool_t in_seg_list(addr) rvm_length_t addr; /* address to search for */
{
    seg_t *seg;
    long i            = 0;
    rvm_bool_t retval = rvm_false;

    /* basic list checks */
    printf("Searching segment list\n");
    if (!chk_list(&seg_root, rvm_true))
        return retval;

    /* check each segment descriptor */
    FOR_ENTRIES_OF(seg_root, seg_t, seg)
    {
        i++;
        if (in_seg(addr, seg, i))
            retval = rvm_true;
    }

    return retval;
}
/* locate an address in change tree */
rvm_bool_t in_seg_dict(addr, seg_dict,
                       n) rvm_length_t addr; /* address to search for */
seg_dict_t *seg_dict; /* segment dictionary entry */
long n;
{
    char *seg_name;
    rvm_bool_t retval = rvm_false;

    printf("   Searching segment dictionary entry %ld\n", n);
    if (seg_dict->seg != NULL)
        seg_name = seg_dict->seg->dev.name;
    else
        seg_name = seg_dict->dev.name;
    if (seg_name == NULL)
        printf("Searching change tree for UNKNOWN segment at %lx\n",
               (long)seg_dict);
    else
        printf("Searching change tree for %s\n", seg_name);

    if (seg_dict->seg != NULL)
        retval = in_seg(addr, seg_dict->seg, 0);
    if (IN_STRUCT(addr, seg_dict, seg_dict_id)) {
        printf("  ***  Address is in seg_dict at %lx\n", (long)seg_dict);
        retval = rvm_true;
    }
    if (seg_dict->dev.name != NULL)
        if (in_heap(addr, (rvm_length_t)seg_dict->dev.name,
                    seg_dict->dev.name_len)) {
            printf("  ***  Address is in device name of seg_dict at %lx\n",
                   (long)seg_dict);
            retval = rvm_true;
        }

    if (search_dev_region(addr, (dev_region_t *)seg_dict->mod_tree.root))
        retval = rvm_true;

    return retval;
}
/* search log special function descriptor */
rvm_bool_t in_log_special(addr, special,
                          n) rvm_length_t addr; /* address to search for */
log_special_t *special; /* log special descriptor to search */
long n;
{
    rvm_bool_t retval = rvm_false;

    /* see if in descriptor */
    printf("   Searching special function descriptor %ld\n", n);
    if (IN_STRUCT(addr, special, log_special_id)) {
        printf("  ***  Address is in log special function decriptor at %lx\n",
               (long)special);
        retval = rvm_true;
    }

    /* structure specific tests */
    switch (special->rec_hdr.struct_id) {
    case log_seg_id:
        if (in_heap(addr, (rvm_length_t)special->special.log_seg.name,
                    special->special.log_seg.name_len + 1)) {
            printf("  ***  Address is in segment name buffer\n");
            retval = rvm_true;
        }
        break;
    default:
        printf("  Record has unknown struct_id\n");
    }

    return retval;
}
/* search modification range descriptor */
rvm_bool_t in_range(addr, range,
                    n) rvm_length_t addr; /* address to search for */
range_t *range;
long n;
{
    rvm_bool_t retval = rvm_false;

    /* see if in descriptor */
    printf("     Searching range %ld\n", n);
    if (IN_STRUCT(addr, range, range_id)) {
        printf("  ***  Address is in modification range decriptor at %lx\n",
               (long)range);
        retval = rvm_true;
    }

    /* see if in old value save buffer */
    if (in_heap(addr, (rvm_length_t)range->data, range->data_len)) {
        printf("  ***  Address is in data buffer of range descriptor");
        printf(" at %lx\n", (long)range);
        retval = rvm_true;
    }
    if (range->nvaddr != NULL)
        if ((addr >= (rvm_length_t)range->nvaddr) &&
            (addr < ((rvm_length_t)range->nvaddr + range->nv.length))) {
            printf("  ***  Address is in data buffer of range descriptor");
            printf(" at %lx\n", (long)range);
            retval = rvm_true;
        }

    /* check the region ptr */
    if (ADDR_INVALID_OR_NULL(range->region))
        printf("  Range at %lx has bad region ptr\n", (long)range);
    else if (range->region->links.struct_id != region_id) {
        printf("  Region at %lx has invalid struct_id,", (long)range->region);
        printf(" struct_id = %d\n", range->region->links.struct_id);
    }

    return retval;
}
/* search transaction descriptor */
rvm_bool_t in_tid(addr, tid, n) rvm_length_t addr; /* address to search for */
int_tid_t *tid; /* transaction descriptor to search */
long n;
{
    range_t *range;
    long i            = 0;
    rvm_bool_t retval = rvm_false;

    /* see if in descriptor */
    printf("   Searching tid %ld\n", n);
    if (IN_STRUCT(addr, tid, int_tid_id)) {
        printf("    ***  Address is in transaction decriptor at %lx\n",
               (long)tid);
        retval = rvm_true;
    }

    /* see if in search vector */
    if (in_heap(addr, (rvm_length_t)tid->x_ranges,
                tid->x_ranges_alloc * sizeof(range_t *))) {
        printf("    ***  Address is in tid.x_ranges at %lx\n", (long)tid);
        retval = rvm_true;
    }

    /* check range list and range descriptors */
    printf("    Checking modification ranges\n");

    /* need chk_tree function 
    if (!chk_tree(&tid->range_tree,rvm_true))
        printf("  Tid at %x has damaged range tree\n",tid);
    else
*/
    FOR_NODES_OF(tid->range_tree, range_t, range)
    {
        i++;
        if (in_range(addr, range, i))
            retval = rvm_true;
    }

    return retval;
}
/* search a log descriptor */
rvm_bool_t in_log(addr, log, n) rvm_length_t addr; /* address to search for */
log_t *log; /* log descriptor to search */
long n; /* position in list */
{
    long i;
    int_tid_t *tid;
    log_special_t *special;
    rvm_bool_t retval = rvm_false;

    /* see if in descriptor */
    printf("  Searching log %ld\n", n);
    if (IN_STRUCT(addr, log, log_id)) {
        printf("  ***  Address is in log descriptor at %lx\n", (long)log);
        retval = rvm_true;
    }

    /* check device name and raw i/o buffer */
    if (ADDR_INVALID_OR_NULL(log->dev.name))
        printf("  Log descriptor at %lx has bad dev.name\n", (long)log);
    else if (in_heap(addr, (rvm_length_t)log->dev.name, log->dev.name_len)) {
        printf("  ***  Address is in log at %lx device name\n", (long)log);
        retval = rvm_true;
    }
    if (log->dev.raw_io) {
        if (in_heap(addr, (rvm_length_t)log->dev.wrt_buf,
                    log->dev.wrt_buf_len)) {
            printf("  ***  Address is in log at %lx wrt_buf\n", (long)log);
            retval = rvm_true;
        }
    }
    /* check i/o vector and pad buffer */
    if (log->dev.iov_length != 0) {
        if (ADDR_INVALID_OR_NULL(log->dev.iov))
            printf("  Log descriptor at %lx has bad dev.iov ptr\n", (long)log);
        else {
            if (in_heap(addr, (rvm_length_t)log->dev.iov,
                        log->dev.iov_length * sizeof(struct iovec))) {
                printf("  ***  Address is in log at %lx i/o vector\n",
                       (long)log);
                retval = rvm_true;
            }
        }
    }
    if (log->dev.pad_buf_len != 0) {
        if (ADDR_INVALID_OR_NULL(log->dev.pad_buf))
            printf("  Log descriptor at %lx has bad dev.pad_buf ptr\n",
                   (long)log);
        else {
            if (in_heap(addr, (rvm_length_t)log->dev.pad_buf,
                        log->dev.pad_buf_len)) {
                printf("  ***  Address is in log pad buffer at %lx\n",
                       (long)log);
                retval = rvm_true;
            }
        }
    }
    /* check recovery buffers */
    if (ADDR_INVALID_OR_NULL(log->log_buf.buf))
        printf("  Log descriptor at %lx has bad log_buf.malloc_buf ptr",
               (long)log);
    else {
        if (in_heap(addr, (rvm_length_t)log->log_buf.buf,
                    log->log_buf.length)) {
            printf("  ***  Address is in log recovery buffer at %lx\n",
                   (long)log);
            retval = rvm_true;
        }
    }
    if (ADDR_INVALID_OR_NULL(log->log_buf.aux_buf))
        printf("  Log descriptor at %lx has bad log_buf.aux_buf ptr",
               (long)log);
    else {
        if (in_heap(addr, (rvm_length_t)log->log_buf.aux_buf,
                    log->log_buf.aux_length)) {
            printf("  ***  Address is in auxillary buffer log at %lx",
                   (long)log);
            printf(" recovery buffer\n");
            retval = rvm_true;
        }
    }
    /* check tid and flush lists */
    printf("  Checking uncommitted tids\n");
    if (!chk_list(&log->tid_list, rvm_true))
        printf("  Log at %lx has damaged uncommited tid list\n", (long)log);
    else {
        i = 0;
        FOR_ENTRIES_OF(log->tid_list, int_tid_t, tid)
        {
            i++;
            if (in_tid(addr, tid, i))
                retval = rvm_true;
        }
    }
    printf("  Checking flush list\n");
    if (!chk_list(&log->flush_list, rvm_true))
        printf("  Log at %lx has damaged flush list\n", (long)log);
    else {
        i = 0;
        FOR_ENTRIES_OF(log->flush_list, int_tid_t, tid)
        {
            i++;
            if (in_tid(addr, tid, i))
                retval = rvm_true;
        }
    }

    /* check immediate stream list */
    printf("  Checking special list\n");
    if (!chk_list(&log->special_list, rvm_true))
        printf("  Log at %lx has damaged special list\n", (long)log);
    else {
        i = 0;
        FOR_ENTRIES_OF(log->special_list, log_special_t, special)
        {
            i++;
            if (in_log_special(addr, special, i))
                retval = rvm_true;
        }
    }

    /* check segment dictionary */
    if (log->seg_dict_vec != NULL) {
        if (ADDR_INVALID(log->seg_dict_vec))
            printf("  Log descriptor at %lx has bad seg_dict_vec ptr\n",
                   (long)log);
        else {
            printf("  Searching segment dictionary\n");
            if (in_heap(addr, (rvm_length_t)log->seg_dict_vec,
                        log->seg_dict_len * sizeof(seg_dict_t))) {
                printf("  ***  Address is in log at %lx seg_dict_vec\n",
                       (long)log);
                retval = rvm_true;
            }
            for (i = 0; i < log->seg_dict_len; i++)
                if (in_seg_dict(addr, &log->seg_dict_vec[i], i + 1))
                    retval = rvm_true;
        }
    }

    return retval;
}
/* search log list */
rvm_bool_t in_log_list(addr) rvm_length_t addr; /* address to search for */
{
    log_t *log;
    long i            = 0;
    rvm_bool_t retval = rvm_false;

    /* basic list checks */
    printf("Searching log list\n");
    if (!chk_list(&log_root, rvm_true))
        return retval;

    /* check each segment descriptor */
    FOR_ENTRIES_OF(log_root, log_t, log)
    {
        i++;
        if (in_log(addr, log, i))
            retval = rvm_true;
    }

    return retval;
}
/* locate an address in RVM internal structures */
void find_addr(addr) rvm_length_t addr; /* address to search for */
{
    rvm_bool_t retval = rvm_false;

    if (in_free_page_list(addr))
        retval = rvm_true;
    if (in_free_lists(addr))
        retval = rvm_true;
    if (in_region_tree(addr))
        retval = rvm_true;
    if (in_seg_list(addr))
        retval = rvm_true;
    if (in_log_list(addr))
        retval = rvm_true;

    if (!retval)
        printf("\nAddress not found\n");
}
/* test if entry is on list -- more forgiving than chk_list */
void on_list(hdr, addr) list_entry_t *hdr; /* header of list to search */
list_entry_t *addr; /* entry to search for */
{
    list_entry_t *entry; /* current list entry */
    long i = 0;

    if (hdr == NULL) {
        printf("List header is null\n");
        return;
    }
    if (ADDR_INVALID(hdr)) {
        printf("List header address invalid\n");
        return;
    }
    if (hdr->is_hdr != rvm_true) {
        printf("List header invalid\n");
        return;
    }
    if (addr == hdr) {
        printf("Entry is list header\n");
        return;
    }

    if (addr == NULL) {
        printf("Entry is null\n");
        return;
    }
    if (ADDR_INVALID(addr)) {
        printf("Entry address invalid\n");
        return;
    }
    if (addr->is_hdr)
        printf("Entry claims to be a list header\n");

    if (!(((long)hdr->struct_id > (long)struct_first_id) &&
          ((long)hdr->struct_id < (long)struct_last_id)))
        printf("  List header type is not valid, struct_id = %ld\n",
               (long)hdr->struct_id);
    if (!(((long)addr->struct_id > (long)struct_first_id) &&
          ((long)addr->struct_id < (long)struct_last_id)))
        printf("  Entry type is not valid, struct_id = %ld\n",
               (long)addr->struct_id);
    if (hdr->struct_id != addr->struct_id) {
        printf("Entry is not of same type as list -- \n");
        printf("  Entry->struct_id  = %ld\n", (long)addr->struct_id);
        printf("  Header->struct_id = %ld\n", (long)hdr->struct_id);
    }
    if (addr->list.name != hdr)
        printf("Entry claims to be on list %lx\n", (long)addr->list.name);

    if (ADDR_INVALID_OR_NULL(hdr->nextentry)) {
        printf("  List header has invalid nextentry field, ");
        printf("hdr->nextentry = %lx\n", (long)hdr->nextentry);
        return;
    }
    if (ADDR_INVALID_OR_NULL(hdr->preventry)) {
        printf("  List header has invalid preventry field, ");
        printf("hdr->preventry = %lx\n", (long)hdr->nextentry);
    }

    /* check all elements of list */
    entry = hdr->nextentry;
    while (entry->is_hdr != rvm_true) {
        i++;
        if (entry == addr) {
            printf("Entry is number %ld of list\n", i);
            return;
        }
        if (ADDR_INVALID_OR_NULL(entry->nextentry)) {
            printf("Entry %ld has invalid nextentry field, ", i);
            printf("nextentry = %lx\n", (long)entry->nextentry);
            return;
        }
        entry = entry->nextentry;
    }

    printf("Entry not on list\n");
}
#endif /* DEBUG_GDB */
