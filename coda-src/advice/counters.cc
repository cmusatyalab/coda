
#include <util.h>

#include "globals.h"
#include "counters.h"

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include "coda_assert.h"

#ifdef __cplusplus
}
#endif __cplusplus

int CounterDebugging = 101;


/* Internal Counters */
InternalCounter EstablishedConnectionCount;
InternalCounter LostConnectionCount;
InternalCounter TokensAcquiredCount;
InternalCounter TokensExpiredCount;
InternalCounter TokensPendingCount;
InternalCounter SpaceInformationCount;
InternalCounter NetworkServerAccessibleCount;
InternalCounter NetworkServerConnectionWeakCount;
InternalCounter NetworkServerConnectionStrongCount;
InternalCounter NetworkServerInaccessibleCount;
InternalCounter NetworkQualityEstimateCount;
InternalCounter VolumeTransitionCount;
InternalCounter ReconnectionCount;
InternalCounter DataFetchEventCount;
InternalCounter ReintegrationPendingCount;
InternalCounter AdviceReconnectionCount;
InternalCounter AdviceDiscoCacheMissCount;
InternalCounter AdviceReadCacheMissCount;
InternalCounter AdviceWeakCacheMissCount;
InternalCounter AdviceHoardWalkCount;
InternalCounter HoardWalkBeginCount;
InternalCounter HoardWalkStatusCount;
InternalCounter HoardWalkEndCount;
InternalCounter HoardWalkOnCount;
InternalCounter HoardWalkOffCount;
InternalCounter RepairInvokeASRCount;
InternalCounter RepairPendingCount;
InternalCounter RepairCompletedCount;
InternalCounter ReintegrationPendingTokensCount;
InternalCounter ReintegrationEnabledCount;
InternalCounter ReintegrationActiveCount;
InternalCounter ReintegrationCompletedCount;
InternalCounter TaskAvailabilityCount;
InternalCounter TaskUnavailableCount;
InternalCounter ProgramAccessLogAvailableCount;
InternalCounter ReplacementLogAvailableCount;


/*******************************************************************************************
 *      Initcounters -- initializes counters to zero
 *      IncrementCounter -- increments a counter's presented or requested value
 *	PrintCounters -- logs a record of the current counter values
 *******************************************************************************************/


void InitCounter(InternalCounter *counter) {
    counter->arrivedFromVenus = 0;
    counter->duplicates = 0;
    counter->sentToCodaConsole = 0;
    counter->completedByUser = 0;
}

void InitCounters() {
    InitCounter(&EstablishedConnectionCount);
    InitCounter(&LostConnectionCount);
    InitCounter(&TokensAcquiredCount);
    InitCounter(&TokensExpiredCount);
    InitCounter(&TokensPendingCount);
    InitCounter(&SpaceInformationCount);
    InitCounter(&NetworkServerAccessibleCount);
    InitCounter(&NetworkServerInaccessibleCount);
    InitCounter(&NetworkQualityEstimateCount);
    InitCounter(&VolumeTransitionCount);
    InitCounter(&ReconnectionCount);
    InitCounter(&DataFetchEventCount);
    InitCounter(&ReintegrationPendingCount);
    InitCounter(&AdviceReconnectionCount);
    InitCounter(&AdviceDiscoCacheMissCount);
    InitCounter(&AdviceReadCacheMissCount);
    InitCounter(&AdviceWeakCacheMissCount);
    InitCounter(&AdviceHoardWalkCount);
    InitCounter(&HoardWalkBeginCount);
    InitCounter(&HoardWalkStatusCount);
    InitCounter(&HoardWalkEndCount);
    InitCounter(&HoardWalkOnCount);
    InitCounter(&HoardWalkOffCount);
    InitCounter(&RepairInvokeASRCount);
    InitCounter(&RepairPendingCount);
    InitCounter(&RepairCompletedCount);
    InitCounter(&ReintegrationPendingTokensCount);
    InitCounter(&ReintegrationEnabledCount);
    InitCounter(&ReintegrationActiveCount);
    InitCounter(&ReintegrationCompletedCount);
    InitCounter(&TaskAvailabilityCount);
    InitCounter(&TaskUnavailableCount);
}

void IncrementCounter(InternalCounter *counter, int count) {
    CODA_ASSERT(counter != NULL);
    switch (count) {
        case ARRIVED:
	    counter->arrivedFromVenus++;
            break;
        case DUPLICATE:
            counter->duplicates++;
	    break;
        case SENT:
            counter->sentToCodaConsole++;
	    break;
        case COMPLETED:
            counter->completedByUser++;
	    break;
        default:
            CODA_ASSERT(0 == 1);
    }
}

void PrintCounter(char *name, InternalCounter counter) {
    LogMsg(CounterDebugging, LogLevel, LogFile,
	   "    %s: <%d,%d,%d,%d>", 
	   name, 
	   counter.arrivedFromVenus, 
	   counter.duplicates, 
	   counter.sentToCodaConsole, 
	   counter.completedByUser);
}

void PrintCounters() {
    PrintCounter("EstablishedConnectionCount", EstablishedConnectionCount);
    PrintCounter("LostConnectionCount", LostConnectionCount);
    PrintCounter("TokensAcquiredCount", TokensAcquiredCount);
    PrintCounter("TokensExpiredCount", TokensExpiredCount);
    PrintCounter("TokensPendingCount", TokensPendingCount);
    PrintCounter("SpaceInformationCount", SpaceInformationCount);
    PrintCounter("NetworkServerAccessibleCount", NetworkServerAccessibleCount);
    PrintCounter("NetworkServerInaccessibleCount", NetworkServerInaccessibleCount);
    PrintCounter("NetworkQualityEstimateCount", NetworkQualityEstimateCount);
    PrintCounter("VolumeTransitionCount", VolumeTransitionCount);
    PrintCounter("ReconnectionCount", ReconnectionCount);
    PrintCounter("DataFetchEventCount", DataFetchEventCount);
    PrintCounter("ReintegrationPendingCount", ReintegrationPendingCount);
    PrintCounter("AdviceReconnectionCount", AdviceReconnectionCount);
    PrintCounter("AdviceDiscoCacheMissCount", AdviceDiscoCacheMissCount);
    PrintCounter("AdviceReadCacheMissCount", AdviceReadCacheMissCount);
    PrintCounter("AdviceWeakCacheMissCount", AdviceWeakCacheMissCount);
    PrintCounter("AdviceHoardWalkCount", AdviceHoardWalkCount);
    PrintCounter("HoardWalkBeginCount", HoardWalkBeginCount);
    PrintCounter("HoardWalkStatusCount", HoardWalkStatusCount);
    PrintCounter("HoardWalkEndCount", HoardWalkEndCount);
    PrintCounter("HoardWalkOnCount", HoardWalkOnCount);
    PrintCounter("HoardWalkOffCount", HoardWalkOffCount);
    PrintCounter("RepairInvokeASRCount", RepairInvokeASRCount);
    PrintCounter("RepairPendingCount", RepairPendingCount);
    PrintCounter("RepairCompletedCount", RepairCompletedCount);
    PrintCounter("ReintegrationPendingTokensCount", ReintegrationPendingTokensCount);
    PrintCounter("ReintegrationEnabledCount", ReintegrationEnabledCount);
    PrintCounter("ReintegrationActiveCount", ReintegrationActiveCount);
    PrintCounter("ReintegrationCompletedCount", ReintegrationCompletedCount);
    PrintCounter("TaskAvailabilityCount", TaskAvailabilityCount);
    PrintCounter("TaskUnavailableCount", TaskUnavailableCount);
    PrintCounter("ProgramAccessLogAvailableCount", ProgramAccessLogAvailableCount);
    PrintCounter("ReplacementLogAvailableCount", ReplacementLogAvailableCount);
    LogMsg(CounterDebugging, LogLevel, LogFile, "");
}
