#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>

#define CODACONSOLE "/coda/usr/mre/src/coda-src/console/CodaConsole"
#define TIXWISH "/coda/usr/mre/src/coda-src/console/CodaConsole.out"
#define TCL "/usr/local/lib/tcl7.4"
#define TK "/usr/local/lib/tk4.0"

extern char **environ;

toUpper(char *s) {
    int i;

    for (i = 0; i < strlen(s); i++) {
        if (islower(s[i]))
	    s[i] = toupper(s[i]);
    }
}

int main(int argc, char **argv) {
  int toAdviceSrv[2];
  int toConsole[2];
  char *args[2];
  char inputline[256];
  int rc;

  InitFail();

  setenv("TCL_LIBRARY", TCL, 1);
  setenv("TK_LIBRARY", TK, 1);

  args[0] = TIXWISH;
  args[1] = NULL;

  rc = pipe(toAdviceSrv);  
  if (rc != 0) {
    printf("Error creating pipe toAdviceSrv\n"); fflush(stdout); exit(-1);
  }
  rc = pipe(toConsole);
  if (rc != 0) {
    printf("Error creating pipe toConsole\n"); fflush(stdout); exit(-1);
  }

  rc = fork();
  if (rc == -1) { /* error */
    printf("ERROR forking process\n"); fflush(stdout); exit(-1);
  } else if (rc) { /* parent -- advice_srv*/
    FILE *toCONSOLE;
    FILE *fromCONSOLE;

    toCONSOLE = fdopen(toConsole[1], "w");
    fromCONSOLE = fdopen(toAdviceSrv[0], "r");

    fprintf(toCONSOLE, "source /coda/usr/mre/src/coda-src/console/CodaConsole\n");
    fflush(toCONSOLE);

/*
    fprintf(toCONSOLE, "ServerBandwidthEstimateEvent scarlatti 524611\n");
    fflush(toCONSOLE);
    sleep(5);
    fprintf(toCONSOLE, "WeakMissEvent /coda/usr/mre/thesis/dissertation/conclusions.tex program6 6\n");
    fflush(toCONSOLE);  
    sleep(5);
    fprintf(toCONSOLE, "TokenExpiryEvent\n");
    fflush(toCONSOLE);  
*/

    printf("Done!\n"); fflush(stdout);

    while (!feof(fromCONSOLE)) {
      fgets(inputline, 256, fromCONSOLE);
      if (strncmp(inputline, "CheckNetworkConnectivityForFilters", 
		             strlen("CheckNetworkConnectivityForFilters")) == 0) {
	  printf("Received request to CheckNetworkConnectivityForFilters\n"); fflush(stdout);
	  CheckNetworkConnectivityForFilters(fromCONSOLE, toCONSOLE);
      } else {
          printf("%s", inputline);
	  fflush(stdout);
      }
    }
  } else {  /* child -- CodaConsole */
    FILE *fromADSRV;
    FILE *toADSRV;
    int newIn, newOut;

    /* Close the CodaConsole's stdin and redirect it to come from the toConsole[0] file descriptor */
    fclose(stdin);
    newIn = dup(toConsole[0]); 

    fclose(stdout);
    newOut = dup(toAdviceSrv[1]);

    if (execve(TIXWISH, args, environ)) {
      printf("ERROR exec'ing child\n"); fflush(stdout); exit(-1);
    }
  }
}


CheckNetworkConnectivityForFilters(FILE *input, FILE *output) {
  char inputline[256];
  char client[256], clientOriginal[256];
  char server[256], serverOriginal[256];
  int rc;

  fgets(inputline, 256, input);
  sscanf(inputline, "client = %s\n", clientOriginal);
  strcpy(client, clientOriginal);
  toUpper(client);

  fgets(inputline, 256, input);
  while (strncmp(inputline, "END", strlen("END")) != 0) {
      sscanf(inputline, "server = %s\n", serverOriginal);
      strcpy(server, serverOriginal);
      toUpper(server);
      strcat(server, ".CODA.CS.CMU.EDU");

      rc = CheckClient(client,server);
      if (rc == 0)  {
	  /* No filters */
	  fprintf(output, "NaturalNetwork %s\n", serverOriginal);
	  fflush(output);
      }
      else if (rc == 1) {
	  /* Filter exists */
	  fprintf(output, "ArtificialNetwork %s\n", serverOriginal);
	  fflush(output);
      } else {
	  /* Give benefit of doubt */
	  fprintf(output, "NaturalNetwork %s\n", serverOriginal);
	  fflush(output);
      }
      fgets(inputline, 256, input);
  }
  fflush(output);
}
