###  You'll need to replace the following path with something appropriate

set path /afs/cs/project/coda-mre/src/coda-src/guitools

lappend auto_path $path

set Window ""
set user [exec whoami]
set password ""

wm withdraw .

puts "Please enter 1 2 1 2 1 2 and then hit return"
puts "  You'll have to ignore the file and block allocation mismatch"
puts "  It's a consequence of giving the bogus data (1 2 1 2 1 2)."

InitLocks
InitSystemAdministrator
InitPathnamesArray
InitDimensionsArray
InitColorArray
InitDisplayStyleArray
InitVenusLog
InitStatistics
InitServers 
InitEventsArray
InitData
InitLog

set HoardWalk(AdviceInput) ${path}/tests/hoardlist.samplein
set HoardWalk(AdviceOutput) /tmp/foo

HoardWalkAdviceInit

Hoard

HoardWalkAdvice 


