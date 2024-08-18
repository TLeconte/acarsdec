/*
 *  Copyright (c) 2015 Thierry Leconte
 *  Copyright (c) 2024 Thibaut VARENE
 *
 *
 *   This code is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>

#include "acarsdec.h"
#include "netout.h"

#define ERRPFX	"ERROR: UDP: "
#define WARNPFX	"WARNING: UDP: "

// params is "host=xxx,port=yyy"
netout_t *Netoutinit(char *params)
{
	char *param, *sep;
	char *addr = NULL;
	char *port = NULL;
	struct addrinfo hints, *servinfo, *p;
	int sockfd, rv;
	netout_t *netpriv = NULL;

	while ((param = strsep(&params, ","))) {
		sep = strchr(param, '=');
		if (!sep)
			continue;
		*sep++ = '\0';
		if (!strcmp("host", param))
			addr = sep;
		else if (!strcmp("port", param))
			port = sep;
	}


	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	if (port == NULL)
		port = "5555";

	vprerr("UDP: Attempting to resolve '%s:%s'.\n", addr, port);

	if ((rv = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, ERRPFX "Invalid/unknown error '%s' resolving '%s:%s'\n", gai_strerror(rv), addr, port);
		return NULL;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (-1 != sockfd)
			break;	// success
	}

	if (!p) {
		fprintf(stderr, ERRPFX "failed to resolve: '%s:%s'\n", addr, port);
		goto fail;
	}

	netpriv = malloc(sizeof(*netpriv));
	if (!netpriv) {
		perror(NULL);
		goto fail;
	}

	memcpy(&netpriv->netOutputAddr, p->ai_addr, p->ai_addrlen);
	netpriv->netOutputAddrLen = p->ai_addrlen;
	netpriv->sockfd = sockfd;

fail:
	freeaddrinfo(servinfo);
	return netpriv;
}

void Netwrite(const void *buf, size_t count, netout_t *net)
{
	int res;

	if (!net->netOutputAddrLen)
		return;

	res = sendto(net->sockfd, buf, count, 0, (struct sockaddr *)&net->netOutputAddr, net->netOutputAddrLen);
	if (res < 0)
		vprerr(WARNPFX "error on sendto(): %s, ignoring.\n", strerror(errno));
}

void Netexit(netout_t *net)
{
	free(net);
}
