#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <assert.h>
#include <strings.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include "admon.h"
#include "adsrv.h"

#include "conversions.h"

InterestID GetInterestID(char *interestString) {
    InterestID interest;

    assert(interestString != NULL);

    if (strncmp(interestString, "TokensAcquired", strlen("TokensAcquired")) == 0)
      interest = TokensAcquiredID;
    else if (strncmp(interestString, "TokensExpired", strlen("TokensExpired")) == 0)
      interest = TokensExpiredID;
    else if (strncmp(interestString, "ActivityPendingTokens", strlen("ActivityPendingTokens")) == 0)
      interest = ActivityPendingTokensID;
    else if (strncmp(interestString, "SpaceInformation", strlen("SpaceInformation")) == 0)
      interest = SpaceInformationID;
    else if (strncmp(interestString, "ServerAccessible", strlen("ServerAccessible")) == 0)
      interest = ServerAccessibleID;
    else if (strncmp(interestString, "ServerInaccessible", strlen("ServerInaccessible")) == 0)
      interest = ServerInaccessibleID;
    else if (strncmp(interestString, "ServerConnectionStrong", strlen("ServerConnectionStrong")) == 0)
      interest = ServerConnectionStrongID;
    else if (strncmp(interestString, "ServerConnectionWeak", strlen("ServerConnectionWeak")) == 0)
      interest = ServerConnectionWeakID;
    else if (strncmp(interestString, "NetworkQualityEstimate", strlen("NetworkQualityEstimate")) == 0)
      interest = NetworkQualityEstimateID;
    else if (strncmp(interestString, "VolumeTransitionEvent", strlen("VolumeTransitionEvent")) == 0)
      interest = VolumeTransitionEventID;
    else if (strncmp(interestString, "Reconnection", strlen("Reconnection")) == 0)
      interest = ReconnectionID;
    else if (strncmp(interestString, "DataFetchEvent", strlen("DataFetchEvent")) == 0)
      interest = DataFetchEventID;
    else if (strncmp(interestString, "ReadDisconnectedCacheMissEvent", strlen("ReadDisconnectedCacheMissEvent")) == 0)
      interest = ReadDisconnectedCacheMissEventID;
    else if (strncmp(interestString, "WeaklyConnectedCacheMissEvent", strlen("WeaklyConnectedCacheMissEvent")) == 0)
      interest = WeaklyConnectedCacheMissEventID;
    else if (strncmp(interestString, "DisconnectedCacheMissEvent", strlen("DisconnectedCacheMissEvent")) == 0)
      interest = DisconnectedCacheMissEventID;
    else if (strncmp(interestString, "HoardWalkAdviceRequest", strlen("HoardWalkAdviceRequest")) == 0)
      interest = HoardWalkAdviceRequestID;
    else if (strncmp(interestString, "HoardWalkBegin", strlen("HoardWalkBegin")) == 0)
      interest = HoardWalkBeginID;
    else if (strncmp(interestString, "HoardWalkStatus", strlen("HoardWalkStatus")) == 0)
      interest = HoardWalkStatusID;
    else if (strncmp(interestString, "HoardWalkEnd", strlen("HoardWalkEnd")) == 0)
      interest = HoardWalkEndID;
    else if (strncmp(interestString, "HoardWalkPeriodicOn", strlen("HoardWalkPeriodicOn")) == 0)
      interest = HoardWalkPeriodicOnID;
    else if (strncmp(interestString, "HoardWalkPeriodicOff", strlen("HoardWalkPeriodicOff")) == 0)
      interest = HoardWalkPeriodicOffID;
    else if (strncmp(interestString, "ObjectInConflict", strlen("ObjectInConflict")) == 0)
      interest = ObjectInConflictID;
    else if (strncmp(interestString, "ObjectConsistent", strlen("ObjectConsistent")) == 0)
      interest = ObjectConsistentID;
    else if (strncmp(interestString, "ReintegrationPendingTokens", strlen("ReintegrationPendingTokens")) == 0)
      interest = ReintegrationPendingTokensID;
    else if (strncmp(interestString, "ReintegrationEnabled", strlen("ReintegrationEnabled")) == 0)
      interest = ReintegrationEnabledID;
    else if (strncmp(interestString, "ReintegrationActive", strlen("ReintegrationActive")) == 0)
      interest = ReintegrationActiveID;
    else if (strncmp(interestString, "ReintegrationCompleted", strlen("ReintegrationCompleted")) == 0)
      interest = ReintegrationCompletedID;
    else if (strncmp(interestString, "TaskAvailability", strlen("TaskAvailability")) == 0)
      interest = TaskAvailabilityID;
    else if (strncmp(interestString, "TaskUnavailable", strlen("TaskUnavailable")) == 0)
      interest = TaskUnavailableID;
    else if (strncmp(interestString, "InvokeASR", strlen("InvokeASR")) == 0)
      interest = InvokeASRID;
    else {
      printf("GetInterestID: Unrecognized interestString = %s\n", interestString);
      assert(1 == 0);
    }

    return(interest);
}

char *InterestToString(InterestID interest) {
  static char returnString[MAXEVENTLEN];

  switch (interest) {
      case TokensAcquiredID:
        strncpy(returnString, "TokensAcquired", MAXEVENTLEN);
        break;
      case TokensExpiredID:
        strncpy(returnString, "TokensExpired", MAXEVENTLEN);
        break;
      case ActivityPendingTokensID:
        strncpy(returnString, "ActivityPendingTokens", MAXEVENTLEN);
        break;
      case SpaceInformationID:
        strncpy(returnString, "SpaceInformation", MAXEVENTLEN);
        break;
      case ServerAccessibleID:
        strncpy(returnString, "ServerAccessible", MAXEVENTLEN);
        break;
      case ServerConnectionStrongID:
        strncpy(returnString, "ServerConnectionStrong", MAXEVENTLEN);
	break;
      case ServerConnectionWeakID:
        strncpy(returnString, "ServerConnectionWeak", MAXEVENTLEN);
	break;
      case NetworkQualityEstimateID:
        strncpy(returnString, "NetworkQualityEstimate", MAXEVENTLEN);
        break;
      case ServerInaccessibleID:
        strncpy(returnString, "ServerInaccessible", MAXEVENTLEN);
        break;
      case VolumeTransitionEventID:
        strncpy(returnString, "VolumeTransitionEvent", MAXEVENTLEN);
        break;
      case ReconnectionID:
        strncpy(returnString, "Reconnection", MAXEVENTLEN);
        break;
      case DataFetchEventID:
        strncpy(returnString, "DataFetchEvent", MAXEVENTLEN);
        break;
      case ReadDisconnectedCacheMissEventID:
        strncpy(returnString, "ReadDisconnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case WeaklyConnectedCacheMissEventID:
        strncpy(returnString, "WeaklyConnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case DisconnectedCacheMissEventID:
        strncpy(returnString, "DisconnectedCacheMissEvent", MAXEVENTLEN);
        break;
      case HoardWalkAdviceRequestID:
        strncpy(returnString, "HoardWalkAdviceRequest", MAXEVENTLEN);
        break;
      case HoardWalkBeginID:
        strncpy(returnString, "HoardWalkBegin", MAXEVENTLEN);
        break;
      case HoardWalkStatusID:
        strncpy(returnString, "HoardWalkStatus", MAXEVENTLEN);
        break;
      case HoardWalkEndID:
        strncpy(returnString, "HoardWalkEnd", MAXEVENTLEN);
        break;
      case HoardWalkPeriodicOnID:
        strncpy(returnString, "HoardWalkPeriodicOn", MAXEVENTLEN);
        break;
      case HoardWalkPeriodicOffID:
        strncpy(returnString, "HoardWalkPeriodicOff", MAXEVENTLEN);
        break;
      case ObjectInConflictID:
        strncpy(returnString, "ObjectInConflict", MAXEVENTLEN);
        break;
      case ObjectConsistentID:
        strncpy(returnString, "ObjectConsistent", MAXEVENTLEN);
        break;
      case ReintegrationPendingTokensID:
        strncpy(returnString, "ReintegrationPendingTokens", MAXEVENTLEN);
        break;
      case ReintegrationEnabledID:
        strncpy(returnString, "ReintegrationEnabled", MAXEVENTLEN);
        break;
      case ReintegrationActiveID:
        strncpy(returnString, "ReintegrationActive", MAXEVENTLEN);
        break;
      case ReintegrationCompletedID:
        strncpy(returnString, "ReintegrationCompleted", MAXEVENTLEN);
        break;
      case TaskAvailabilityID:
        strncpy(returnString, "TaskAvailability", MAXEVENTLEN);
        break;
      case TaskUnavailableID:
        strncpy(returnString, "TaskUnavailable", MAXEVENTLEN);
        break;
      case InvokeASRID:
        strncpy(returnString, "InvokeASR", MAXEVENTLEN);
        break;
      default:
	fprintf(stderr, "Invalid InterestID = %d\n", interest);
	fflush(stderr);
        assert(1 == 0);
  }

  return(returnString);
}


HoardCommandID GetHoardCommandID(char *commandString) {
    HoardCommandID command;

    assert(commandString != NULL);

    if (strncmp(commandString, "add", strlen("add")) == 0)
      command = AddCMD;
    else if (strncmp(commandString, "clear", strlen("clear")) == 0)
      command = ClearCMD;
    else if (strncmp(commandString, "delete", strlen("delete")) == 0)
      command = DeleteCMD;
    else if (strncmp(commandString, "list", strlen("list")) == 0)
      command = ListCMD;
    else if (strncmp(commandString, "off", strlen("off")) == 0)
      command = OffCMD;
    else if (strncmp(commandString, "on", strlen("on")) == 0)
      command = OnCMD;
    else if (strncmp(commandString, "walk", strlen("walk")) == 0)
      command = WalkCMD;
    else if (strncmp(commandString, "verify", strlen("verify")) == 0)
      command = VerifyCMD;
    else
      assert(1 == 0);

    return(command);
}


char *HoardCommandToString(HoardCommandID command) {
  static char returnString[MAXCOMMANDLEN];

  switch (command) {
      case AddCMD:
        strncpy(returnString, "add", MAXCOMMANDLEN);
        break;
      case ClearCMD:
        strncpy(returnString, "clear", MAXCOMMANDLEN);
        break;
      case DeleteCMD:
        strncpy(returnString, "delete", MAXCOMMANDLEN);
        break;
      case ListCMD:
        strncpy(returnString, "list", MAXCOMMANDLEN);
        break;
      case OffCMD:
        strncpy(returnString, "off", MAXCOMMANDLEN);
        break;
      case OnCMD:
        strncpy(returnString, "on", MAXCOMMANDLEN);
        break;
      case WalkCMD:
        strncpy(returnString, "walk", MAXCOMMANDLEN);
        break;
      case VerifyCMD:
        strncpy(returnString, "verify", MAXCOMMANDLEN);
        break;
      default:
	assert(1 == 0);
  }

  return(returnString);
}



MetaInfoID GetMetaInfoID(char *metaString) {
    MetaInfoID metaInfo;

    assert(metaString != NULL);

    if (strncmp(metaString, "n", strlen("n")) == 0)
      metaInfo = NoneMETA;
    else if (strncmp(metaString, "c", strlen("c")) == 0)  // Matches either c or c+
      metaInfo = ChildrenPlusMETA;
    else if (strncmp(metaString, "d", strlen("d")) == 0)  // Matches either d or d+
      metaInfo = DescendantsPlusMETA;
    else
      assert(1 == 0);

    return(metaInfo);
}


char *MetaInfoIDToString(MetaInfoID meta) {
  static char returnString[MAXMETALEN];

  switch(meta) {
      case NoneMETA:
        strncpy(returnString, "n", MAXMETALEN);
        break;
      case ChildrenMETA:
        strncpy(returnString, "c", MAXMETALEN);
        break;
      case ChildrenPlusMETA:
        strncpy(returnString, "c+", MAXMETALEN);
        break;
      case DescendantsMETA:
        strncpy(returnString, "d", MAXMETALEN);
        break;
      case DescendantsPlusMETA:
        strncpy(returnString, "d+", MAXMETALEN);
        break;
      default:
	assert(1 == 0);
  }

  return(returnString);
}
