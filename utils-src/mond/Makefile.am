## Process this file with automake to produce Makefile.in

# this code hasn't been built in a very long time
noinst_PROGRAMS = mond mondmon parselog grab unwind adviceunwind sqlunlock
noinst_SCRIPTS = classify_volumes.sh trunclog.sh
BUILT_SOURCES = db.c

mond_SOURCES	      = bbuf.c bbuf.h data.c data.h vargs.c vargs.h mond.c \
			wind.c version.c version.h mondutil.c mondutil.h
mondmon_SOURCES	      = mondmon.c
parselog_SOURCES      = parselog.c datalog.c
grab_SOURCES	      = wind.c data.c data.h vargs.c vargs.h
unwind_SOURCES	      = unwind.c db.h vargs.c datalog.c
nodist_unwind_SOURCES = db.c
adviceunwind_SOURCES  = advice_unwind.c db.h mondgen.h vargs.c vargs.h \
			advice_parser.c
nodist_adviceunwind_SOURCES = db.c
EXTRA_DIST = db.ec dummy.c report.h delete.sql dropall.sql dropidx.sql \
	     index.sql listentries.sql modifysetup.sql README setup.sql

CLEANFILES = $(BUILT_SOURCES)

ESQL=$(SCYLLADIR)/bin/esql
SCYLLADIR=/afs/cs.cmu.edu/misc/scylla/@sys/omega
AM_CPPFLAGS = -DVERSION_CONTROL -DMOND -I$(SCYLLADIR)/incl

RPCLIBS= $(LIBRPC2) $(LIBLWP)
SCYLLALIBS=$(SCYLLADIR)/lib/libsql.a		\
	$(SCYLLADIR)/lib/libutil.a		\
	$(SCYLLADIR)/lib/librds.a
LDADD = $(SCYLLALIBS) $(RPCLIBS)

db.c: db.ec
	${ESQL} ${CFLAGS} -e db.ec

