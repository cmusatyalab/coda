class progent {
    friend int ProgramPriorityFN(bsnode *, bsnode *);
    friend void PrintPWDB(char *);
    friend int IsProgramUnderWatch(char *);

  
    char *program;
    bsnode queue_handle;  /* link for the program queue */

  public:
    progent(char *Program);
    progent(progent&);
    operator=(progent&);
    ~progent();

    void print(FILE *f);
};

class dataent {
    friend int DataAreaPriorityFN(bsnode *, bsnode *);
    friend void PrintUADB(char *);
    friend int IsProgramAccessingUserArea(VolumeId volume);

    VolumeId volume;
    bsnode queue_handle;

  public:
    dataent(VolumeId vol);
    dataent(dataent&);
    operator=(dataent&);
    ~dataent();

    void print(FILE *f);
};

extern void InitPWDB();
extern void InitUADB();

extern void ParseProgramDefinitions(char *);
extern void ParseDataDefinitions(char *);
extern void ProcessProgramAccessLog(char *, char *);

