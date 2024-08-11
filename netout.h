#ifndef netout_h
#define netout_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

typedef struct {
	int sockfd;
	struct sockaddr_storage netOutputAddr;
	socklen_t netOutputAddrLen;
} netout_t;

netout_t *Netoutinit(char *params);
void Netwrite(const void *buf, size_t count, netout_t *net);
void Netexit(netout_t *net);

#endif /* netout_h */
