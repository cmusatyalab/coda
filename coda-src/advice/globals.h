/* Location of TCL/TK/TIX */
#ifdef __BSD44__
#define TCL "/usr/local/lib/tcl7.4"
#define TK "/usr/local/lib/tk4.0"
#define TIX "/usrl/ocal/lib/tix4.1"
#else
#define TCL "/usr/lib/tcl8.0"
#define TK "/usr/lib/tk8.0"
#define TIX "/usr/lib/tix4.1.8.0"
#endif /* __linux__ */


/* Local constants */
const int smallStringLength = 64;

/* Venus Version Information */
extern int VenusMajorVersionNumber;
extern int VenusMinorVersionNumber;

/* Command Line Arguments */
extern int debug;
extern int verbose;
extern int AutoReply;
extern int LogLevel;
extern int StackChecking;


/* Logging Information */extern FILE *LogFile;
extern char LogFileName[];
extern int LogLevel;
extern FILE *EventFile;
extern char EventFileName[];

/* Log Levels */
extern int EnterLeaveMsgs;
extern int RPCdebugging;	/* Defined in rpcs.cc */
extern int CounterDebugging;	/* Defined in counters.cc */


/* Storage Information */
extern char BaseDir[];
extern char WorkingDir[];
extern char tmpDir[];

/* Error Information */
extern int errno;
extern char error_msg[];

/* Process Information */
extern int thisPGID;
extern int thisPID;
extern int childPID;
extern int interfacePID;

/* User Information */
extern char UserName[];
extern int uid;

/* Host Information */
extern char ShortHostName[];
extern char HostName[];

/* Connection Information */
extern int WeLostTheConnection;

/* Synchronization Variables */
extern char shutdownSync;
extern char dataSync;
extern char programlogSync;
extern char replacementlogSync;
extern char initialUserSync;
extern char userSync;
extern char workerSync;

extern char discomissSync;
extern char weakmissSync;
extern char readmissSync;
extern char miscSync;
extern char hoardwalkSync;

/* Return Answer Variables */
extern int weakmissAnswer;
extern int readmissAnswer;

/* RPC Parameter Values */
extern char ProgramAccessLog[];
extern char ReplacementLog[];

/* Function Declarations */
extern void InitLogFile();
extern void InitEventFile();
extern void InitPGID();
extern void InitEnvironment();
extern void InitHostName();
extern void PrintUsage();
extern void CommandLineArgs(int, char **);
extern void InitiateNewUser();
