# RVM Segment Loader

*Designed and programmed by David C. Steere*

The RVM segment loader provides a mechanism that allows a load map to be created for
a segment. Regions can then be mapped to specific virtual memory addresses or to
virtual memory allocated by RVM in a single library call. There are only two library
functions: `rvm_create_segment` and `rvm_load_segment`.

To use the segment loader, a segment header must be created by `rvm_create_segment`.
The header, which occupies the first page of the segment, contains the load map for
regions defined within the segment, and a version identifier so that `rvm_load_segment`
can recognize suitable segments. All region lengths must be integral multiple of the
page size of the machine, which is available as the macro `RVM_PAGE_SIZE`. Virtual
memory addresses must be page-aligned.

The load map is prepared in a vector of descriptors for the desired regions (type
`rvm_region_def_t`). The descriptors specify just two parameters: the region length
and the virtual memory address where the region is to be loaded. The offsets of the
regions in the segment are assigned by `rvm_create_segment`. If pointers are included
in the data, a fixed virtual memory address must be specified so that the loader will
place the region there at each loading. If there are no pointers, the segment loader
can be instructed to allocate space for the region by specifying an address of zero.
The segment loader will make virtual memory addressable as necessary when loading.

The virtual memory address for the first region should be specified far enough beyond
the applications break point so that the program can grow during development. Otherwise,
there will have to be a way to modify the pointers or rebuild the structures.

The segment header establishes the load map for only that segment. If multiple segments
are used, each must have its own header, and `rvm_load_segment` must be called for each.

The segment loader is used by the RDS allocator (see Chapter [RDS, A Dynamic Heap Allocator](rds_allocator.md))
to automate loading of a persistent heap region and a statically allocated region.
A utility program is available to create the segment header and initialize the heap
so that it is easily loaded.

The header file giving all necessary definitions for the RVM segment loader is included
in Appendix [C Declaration of RVM](rvm_headers.md).

- [rvm_create_segment](manpages/rvm_create_segment.3.md)
- [rvm_load_segment](manpages/rvm_load_segment.3.md)
