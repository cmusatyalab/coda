#define TMPREPLACELIST "/tmp/replacelist"
#define MAXSTATUSREPLACEMENTS 10
#define MAXDATAREPLACEMENTS 10

extern void ParseReplacementLog(char *);
extern void PrintGhostDB();
extern void OutputReplacementStatistics();
extern int Find(char *);

extern char GhostDB[];
extern struct Lock GhostLock;
