#!/usr/bin/python
#                            Coda File System
#                               Release 6
# 
#           Copyright (c) 1987-2003 Carnegie Mellon University
#                   Additional copyrights listed below
# 
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public Licence Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
#
#########################################################################

LOGDIR="/var/log/smon2"
IMGDIR="/home/httpd/html/smon2"
IMGURL="/smon2/"
RRDTOOL="/usr/local/rrdtool-1.0.4/bin/rrdtool"

srvmap = [ "verdi", "mozart", "marais", "vivaldi", "mahler", "viotti",
	   "massenet", "strauss", "tye", "tallis", "haydn", "holst" ]

##########################################################################

import cgi, sys, traceback, os

print """\
Content-type: text/html

<HTML><HEAD><TITLE>Server statistics</TITLE></HEAD>
<BODY BGCOLOR="#FFFFFF">
"""
sys.stderr = sys.stdout
hell="freezing"

statmap = {
    "clients" 	: ( "Workstations", """\
DEF:aclnt=%(srv)s.rrd:aclnt:AVERAGE \
DEF:clnts=%(srv)s.rrd:clnts:AVERAGE \
AREA:aclnt#f00000:\"Active Clients\" \
LINE2:clnts#000000:\"Clients\"""" ),

    "conns" 	: ( "RPC2 connections", """\
DEF:conns=%(srv)s.rrd:conns:AVERAGE \
LINE2:conns#000000:\"connections\"""" ),

    "rpcbytes"  : ( "RPC2 data transferred", """\
DEF:rbsnd=%(srv)s.rrd:rbsnd:AVERAGE \
DEF:rbrcv=%(srv)s.rrd:rbrcv:AVERAGE \
AREA:rbsnd#ff0000:\"Sent rpc bytes\" \
LINE2:rbrcv#00ff00:\"Received rpc bytes\"""" ),

    "rpcpkts" 	: ( "RPC2 packet statistics", """\
DEF:rpsnd=%(srv)s.rrd:rpsnd:AVERAGE \
DEF:rplst=%(srv)s.rrd:rplst:AVERAGE \
DEF:rprcv=%(srv)s.rrd:rprcv:AVERAGE \
DEF:rperr=%(srv)s.rrd:rperr:AVERAGE \
LINE2:rpsnd#00f000:\"Sent packets\" \
LINE2:rplst#f0f000:\"Lost packets\" \
LINE2:rprcv#0000ff:\"Received packets\" \
LINE2:rperr#ff0000:\"Bogus packets (per second)\"""" ),

    "xferops"	: ( "Store/Fetch operations", """\
DEF:ftotl=%(srv)s.rrd:ftotl:AVERAGE \
DEF:fdata=%(srv)s.rrd:fdata:AVERAGE \
DEF:stotl=%(srv)s.rrd:stotl:AVERAGE \
DEF:sdata=%(srv)s.rrd:sdata:AVERAGE \
DEF:nrpcs=%(srv)s.rrd:nrpcs:AVERAGE \
CDEF:fattr=ftotl,fdata,- \
CDEF:sattr=stotl,sdata,- \
AREA:fdata#ff0000:\"FetchData\" \
STACK:fattr#c00000:\"FetchAttr\" \
LINE2:sdata#00ff00:\"StoreData\" \
STACK:sattr#00c000:\"StoreAttr\" \
LINE2:nrpcs#000000:\"Total RPCs (per second)\"""" ),

    "xferbytes"	: ( "Store/Fetch bytes", """\
--base 1024 \
DEF:fsize=%(srv)s.rrd:fsize:AVERAGE \
DEF:ssize=%(srv)s.rrd:ssize:AVERAGE \
AREA:fsize#ff0000:\"Fetch Size\" \
LINE2:ssize#00ff00:\"Store Size\"""" ),

    "xferrates"	: ( "Store/Fetch datarates", """\
DEF:frate=%(srv)s.rrd:frate:AVERAGE \
DEF:srate=%(srv)s.rrd:srate:AVERAGE \
LINE2:frate#ff0000:\"Fetch Rate\" \
LINE2:srate#00ff00:\"Store Rate\"""" ),

    "cpu"	: ( "System CPU usage", """\
--upper-limit 100 --rigid \
DEF:sscpu=%(srv)s.rrd:sscpu:AVERAGE \
DEF:sucpu=%(srv)s.rrd:sucpu:AVERAGE \
DEF:sncpu=%(srv)s.rrd:sncpu:AVERAGE \
DEF:sicpu=%(srv)s.rrd:sicpu:AVERAGE \
AREA:sscpu#ff0000:\"System\" \
STACK:sucpu#0000ff:\"User\" \
STACK:sncpu#00f000:\"Nice\" \
STACK:sicpu#c0c0c0:\"Idle\"""" ),

    "times"	: ( "Server CPU usage", """\
DEF:stime=%(srv)s.rrd:stime:AVERAGE \
DEF:utime=%(srv)s.rrd:utime:AVERAGE \
AREA:stime#ff0000:\"System Time\" \
STACK:utime#0000ff:\"User Time (0.01s)\"""" ),

    "vmusage"	: ( "Server memory usage", """\
--base 1024 \
DEF:vmsiz=%(srv)s.rrd:vmsiz:AVERAGE \
DEF:vmdat=%(srv)s.rrd:vmdat:AVERAGE \
DEF:vmrss=%(srv)s.rrd:vmrss:AVERAGE \
CDEF:siz=vmsiz,1024,* \
CDEF:dat=vmdat,1024,* \
CDEF:rss=vmrss,1024,* \
AREA:siz#c0c0c0:\"Process size\" \
LINE2:dat#ff0000:\"Data size\" \
LINE2:rss#000000:\"Resident set size\"""" ),

    "faults"	: ( "Page faults", """\
DEF:mjflt=%(srv)s.rrd:mjflt:AVERAGE \
DEF:mnflt=%(srv)s.rrd:mnflt:AVERAGE \
DEF:nswap=%(srv)s.rrd:nswap:AVERAGE \
AREA:mjflt#ff0000:\"Major Faults\" \
STACK:mnflt#00f000:\"Minor Faults\" \
LINE2:nswap#000000:\"nr.Swaps\"""" ),

    "ioops"	: ( "System IO operations", """\
DEF:totio=%(srv)s.rrd:totio:AVERAGE \
LINE2:totio#000000:\"IO-ops (blocks/sec)\"""" ),

    "disks" : ( "Disk space available", """\
DEF:d1avl=%(srv)s.rrd:d1avl:AVERAGE \
DEF:d2avl=%(srv)s.rrd:d2avl:AVERAGE \
DEF:d3avl=%(srv)s.rrd:d3avl:AVERAGE \
DEF:d4avl=%(srv)s.rrd:d4avl:AVERAGE \
DEF:d1tot=%(srv)s.rrd:d1tot:AVERAGE \
DEF:d2tot=%(srv)s.rrd:d2tot:AVERAGE \
DEF:d3tot=%(srv)s.rrd:d3tot:AVERAGE \
DEF:d4tot=%(srv)s.rrd:d4tot:AVERAGE \
DEF:d1now=%(srv)s.rrd:d1avl:MAX \
DEF:d2now=%(srv)s.rrd:d2avl:MAX \
DEF:d3now=%(srv)s.rrd:d3avl:MAX \
DEF:d4now=%(srv)s.rrd:d4avl:MAX \
CDEF:d1=d1avl,d1tot,/,100,* \
CDEF:d2=d2avl,d2tot,/,100,* \
CDEF:d3=d3avl,d3tot,/,100,* \
CDEF:d4=d4avl,d4tot,/,100,* \
LINE2:d1#ff0000:\"disk1\" \
LINE2:d2#00ffff:\"disk2\" \
LINE2:d3#00ff00:\"disk3\" \
LINE2:d4#ffff00:\"disk4 (%%)\" \
COMMENT:\\s \
COMMENT:\"Number of blocks available\" \
GPRINT:d1now:LAST:\"Disk1\\: %%.0lf\" \
GPRINT:d2now:LAST:\"Disk2\\: %%.0lf\" \
GPRINT:d3now:LAST:\"Disk3\\: %%.0lf\" \
GPRINT:d4now:LAST:\"Disk4\\: %%.0lf\"""" )
}

timemap = {
    "1h" : " -s-1h ",
    "2h" : " -s-2h ",
    "4h" : " -s-4h ",
    "8h" : " -s-8h ",
    "1d" : " -s-1d ",
    "2d" : " -s-2d ",
    "w"  : " -s-1w ",
    "m"  : " -s-1month ",
    "y"  : " -s-1y "
}

try:
    form = cgi.FieldStorage()
    if not form.has_key("servers") or not form.has_key("stats"):
	raise hell
    
    servers = form["servers"]
    stats   = form["stats"]
    period  = form["period"]

    if type(servers) != type([]):
	servers = [ servers ]
    if type(stats) != type([]):
	stats = [ stats ]
    if type(period) == type([]):
	raise hell

    for server in servers:
	if not server.value in srvmap:
	    raise hell, server.value

    period  = timemap[period.value]
    if form.has_key("logscale"):
	logscale = "--logarithmic "
	lapp = "_l"
    else:
	logscale = ""
	lapp = ""

    os.chdir(LOGDIR)
    rrdtool = os.popen("%s - > /dev/null 2>&1" % RRDTOOL, "w")

    for stat in stats:
	for server in servers:
	    srv = server.value
	    img = "%s_%s%s.gif" % ( srv, stat.value, lapp )
	    desc, ops = statmap[stat.value]
	    cmd = "graph %s/%s -w 640 -h 200 %s" % ( IMGDIR, img, period ) + logscale + ops % vars() + "\n"
	    rrdtool.write(cmd)
	    rrdtool.flush()
	    print "<H2>%s - %s</H2>" % ( srv, desc )
	    #print cmd
	    print "<IMG SRC=\"%s%s\">" % ( IMGURL, img )
    rrdtool.close()

except:
    print """

<PRE>"""
    traceback.print_exc()

print "</BODY></HTML>"
