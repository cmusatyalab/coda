#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char **argv) 
{

	int sec,rc;
	struct timeval t;
	int i;

	rc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	printf ("socket() returns %d\n", rc);
	if (rc == -1)
		return(1);

	if ( argc !=2 ) {
		printf("Usage: %s timeout\n", argv[0]);
		return(1);
	}

	for (i = 0; i < 20; i++) {
		t.tv_sec = atoi(argv[1]);
		t.tv_usec = 0;
		rc = select(0, NULL, NULL, NULL, &t);

		printf("waited %d, returned %d (timeout now %d)\n", atoi(argv[1]), rc, t.tv_sec);
		
		if ( rc ) {
			printf("select: ");
		}
	}
}
