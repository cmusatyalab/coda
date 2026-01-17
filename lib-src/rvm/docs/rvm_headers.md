# C Declaration for RVM

This appendix contains listings of the C header files used to
construct programs with RVM.
The following files are included:

- [**rvm.h**][rvm_h]: main RVM library declarations.
- [**rvm_statistics.h**][rvm_statistics_h]: RVM statistics declarations.
- [**rds.h**][rds_h]: recoverable heap allocator declarations.
- [**rvm_segment.h**][rvm_segment_h]: segment loader declarations.

The header `rvm.h` is required for all uses of RVM.  The other
should be included as needed.  The order of inclusion is not
important, but Unix system files are often included first.  Should the
files be included more than once, only the first is processed.

[rvm_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/rvm/include/rvm/rvm.h
[rvm_statistics_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/rvm/include/rvm/rvm_statistics.h
[rds_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/rvm/include/rvm/rds.h
[rvm_segment_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/rvm/include/rvm/rvm_segment.h
