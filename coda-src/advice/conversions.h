
#define MAXEVENTLEN 64
#define MAXCOMMANDLEN 8
#define MAXMETALEN 4

extern InterestID GetInterestID(char *);
extern HoardCommandID GetHoardCommandID(char *);
extern MetaInfoID GetMetaInfoID(char *);

extern char *InterestToString(InterestID);
extern char *HoardCommandToString(HoardCommandID);
extern char *MetaInfoIDToString(MetaInfoID);
