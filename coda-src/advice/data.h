extern const int timeLength;

extern void InitUserData();
extern void InitDataDirectory();
extern void InitializeDirectory(char *, char *);
extern void CreateDataDirectory(char *);
extern void CreateREADMEFile(char *);
extern void SetAccessLists(char *, char *);
extern int MoveFile(char *, char *, char*);
extern char *GetDataFile(char *);
