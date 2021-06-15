---
title: Makefile
---
``` makefile
HFILES = $(INCLDIR)/rpc2.h $(INCLDIR)/se.h  $(INCLDIR)/lwp.h

LIBS = -lrpc2 -lse -loldlwp

examples: authcomp_clnt authcomp_srv rtime_clnt rtime_srv rcat_clnt rcat_srv\
        multi_rtime_clnt multi_rcat_clnt

OCLIENT1 = rtime_clnt.o rtime.client.o
OSERVER1 = rtime_srv.o rtime.server.o

OCLIENT2 = rcat_clnt.o rcat.client.o
OSERVER2 = rcat_srv.o rcat.server.o

OCLIENT3 = authcomp_clnt.o  auth.client.o comp.client.o
OSERVER3 = authcomp_srv.o  auth.server.o comp.server.o

OCLIENT4 = multi_rtime_clnt.o rtime.client.o

OCLIENT5 = multi_rcat_clnt.o rcat.client.o


rtime_clnt: $(OCLIENT1)
    $(CC) $(CFLAGS) $(OCLIENT1) $(LIBS) -o rtime_clnt

rtime_srv: $(OSERVER1)
    $(CC) $(CFLAGS) $(OSERVER1) $(LIBS) -o rtime_srv

rcat_clnt: $(OCLIENT2)
    $(CC) $(CFLAGS) $(OCLIENT2) $(LIBS) -o rcat_clnt

rcat_srv: $(OSERVER2)
    $(CC) $(CFLAGS) $(OSERVER2) $(LIBS) -o rcat_srv

authcomp_clnt: $(OCLIENT3)
    $(CC) $(CFLAGS) $(OCLIENT3) $(LIBS) -o authcomp_clnt

authcomp_srv: $(OSERVER3)
    $(CC) $(CFLAGS) $(OSERVER3) $(LIBS) -o authcomp_srv

multi_rtime_clnt: $(OCLIENT4)
    $(CC) $(CFLAGS) $(OCLIENT4) $(LIBS) -o multi_rtime_clnt

multi_rcat_clnt: $(OCLIENT5)
    $(CC) $(CFLAGS) $(OCLIENT5) $(LIBS) -o multi_rcat_clnt


$(OCLIENT1) $(OSERVER1): rtime.h $(HFILES)

$(OCLIENT2) $(OSERVER2): rcat.h $(HFILES)

$(OCLIENT3) $(OSERVER3): auth.h comp.h $(HFILES)


auth.h auth.server.c auth.client.c: auth.rpc  $(RP2GEN)
    $(RP2GEN) auth.rpc

comp.h comp.server.c comp.client.c: comp.rpc  $(RP2GEN)
    $(RP2GEN) comp.rpc

rtime.h rtime.server.c rtime.client.c: rtime.rpc $(RP2GEN)
    $(RP2GEN) rtime.rpc

rcat.h rcat.server.c rcat.client.c: rcat.rpc $(RP2GEN)
    $(RP2GEN) rcat.rpc
```
