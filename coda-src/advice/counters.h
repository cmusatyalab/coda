#define ARRIVED 0
#define DUPLICATE 1
#define SENT 2
#define COMPLETED 3


typedef struct {
    int arrivedFromVenus;  	/* ARRIVED counts */
    int duplicates;		/* DUPLICATE counts */
    int sentToCodaConsole;	/* SENT counts */
    int completedByUser;	/* COMPLETED counts */
} InternalCounter;


extern InternalCounter EstablishedConnectionCount;
extern InternalCounter LostConnectionCount;
extern InternalCounter TokensAcquiredCount;
extern InternalCounter TokensExpiredCount;
extern InternalCounter TokensPendingCount;
extern InternalCounter SpaceInformationCount;
extern InternalCounter NetworkServerAccessibleCount;
extern InternalCounter NetworkServerConnectionWeakCount;
extern InternalCounter NetworkServerConnectionStrongCount;
extern InternalCounter NetworkServerInaccessibleCount;
extern InternalCounter NetworkQualityEstimateCount;
extern InternalCounter VolumeTransitionCount;
extern InternalCounter ReconnectionCount;
extern InternalCounter DataFetchEventCount;
extern InternalCounter ReintegrationPendingCount;
extern InternalCounter AdviceReconnectionCount;
extern InternalCounter AdviceDiscoCacheMissCount;
extern InternalCounter AdviceReadCacheMissCount;
extern InternalCounter AdviceWeakCacheMissCount;
extern InternalCounter AdviceHoardWalkCount;
extern InternalCounter HoardWalkBeginCount;
extern InternalCounter HoardWalkStatusCount;
extern InternalCounter HoardWalkEndCount;
extern InternalCounter HoardWalkOnCount;
extern InternalCounter HoardWalkOffCount;
extern InternalCounter RepairInvokeASRCount;
extern InternalCounter RepairPendingCount;
extern InternalCounter RepairCompletedCount;
extern InternalCounter ReintegrationPendingTokensCount;
extern InternalCounter ReintegrationEnabledCount;
extern InternalCounter ReintegrationActiveCount;
extern InternalCounter ReintegrationCompletedCount;
extern InternalCounter TaskAvailabilityCount;
extern InternalCounter TaskUnavailableCount;
extern InternalCounter ProgramAccessLogAvailableCount;
extern InternalCounter ReplacementLogAvailableCount;


extern void InitCounters();
extern void IncrementCounter(InternalCounter *, int);
extern void PrintCounters();
