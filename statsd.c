/*
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "acarsdec.h"
#include "statsd.h"

#define STATSD_UDP_BUFSIZE	1432	///< udp buffer size. Untold rule seems to be that the datagram must not be fragmented.
#define STATSD_NAMESPACE	"acarsdec."

#define ERRPFX	"ERROR: STATSD: "
#define WARNPFX	"WARNING: STATSD: "

static struct {
	char *namespace;		///< statsd namespace prefix (dot-terminated)
	struct sockaddr_storage ai_addr;
	socklen_t ai_addrlen;
	int sockfd;
} statsd_runtime = {};

/**
 * Resolve remote host and open socket.
 * @param parms host=foo,port=1234
 * @param idstation station id
 * @return socket fd or negative error
 */
int statsd_init(char *params, const char *idstation)
{
	const char *host = NULL, *port = NULL;
	char *param, *sep;
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int ret;

	while ((param = strsep(&params, ","))) {
		sep = strchr(param, '=');
		if (!sep)
			continue;
		*sep++ = '\0';
		if (!strcmp("host", param))
			host = sep;
		else if (!strcmp("port", param))
			port = sep;
	}

	if (!host || !port) {
		fprintf(stderr, ERRPFX "invalid parameters\n");
		return -1;
	}

	// obtain address(es) matching host/port
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	ret = getaddrinfo(host, port, &hints, &result);
	if (ret) {
		fprintf(stderr, ERRPFX "getaddrinfo: %s\n", gai_strerror(ret));
		return -1;
	}

	// try each address until one succeeds
	for (rp = result; rp; rp = rp->ai_next) {
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (-1 != sockfd)
			break;	// success
	}

	if (!rp) {
		fprintf(stderr, ERRPFX "Could not reach server\n");
		goto cleanup;
	}

	memcpy(&statsd_runtime.ai_addr, rp->ai_addr, rp->ai_addrlen);	// ai_addrlen is guaranteed to be <= sizeof(sockaddr_storage)
	statsd_runtime.ai_addrlen = rp->ai_addrlen;

	ret = strlen(STATSD_NAMESPACE);
	if (idstation)
		ret += strlen(idstation);
	statsd_runtime.namespace = malloc(ret + 2);
	if(!statsd_runtime.namespace) {
		perror("statsd");
		goto cleanup;
	}
	sep = stpcpy(statsd_runtime.namespace, STATSD_NAMESPACE);
	if (idstation) {
		strcpy(sep, idstation);
		statsd_runtime.namespace[ret++] = '.';
		statsd_runtime.namespace[ret] = '\0';
	}

	statsd_runtime.sockfd = sockfd;

cleanup:
	freeaddrinfo(result);

	return sockfd;
}

#ifdef DEBUG
static int statsd_validate(const char * stat)
{
	const char * p;
	for (p = stat; *p; p++) {
		switch (*p) {
			case ':':
			case '|':
			case '@':
				return (-1);
			default:
				;	// nothing
		}
	}

	return 0;
}
#endif


/**
 * Update StatsD metrics.
 * @param pfx a prefix prepended to each metric name
 * @param metrics an array of metrics to push to StatsD
 * @param nmetrics the array size
 * @return exec status
 * @warning not thread-safe.
 */
int statsd_update(const char *pfx, const struct statsd_metric * const metrics, const unsigned int nmetrics)
{
	static char sbuffer[STATSD_UDP_BUFSIZE];
	const char *mtype;
	char * buffer;
	bool zerofirst;
	int ret;
	ssize_t sent;
	size_t avail;
	unsigned int i;

	if (!statsd_runtime.namespace)
		return 0;

	buffer = sbuffer;
	avail = STATSD_UDP_BUFSIZE;

	for (i = 0; i < nmetrics; i++) {
#ifdef DEBUG
		if ((statsd_validate(metrics[i].name) != 0)) {
			fprintf(stderr, WARNPFX "ignoring invalid name \"%s\"", metrics[i].name);
			continue;
		}
#endif

		zerofirst = false;

		switch (metrics[i].type) {
			case STATSD_LGAUGE:
				mtype = "g";
				if (metrics[i].value.l < 0)
					zerofirst = true;
				break;
			case STATSD_FGAUGE:
				mtype = "g";
				if (metrics[i].value.f < 0.0F)
					zerofirst = true;
				break;
			case STATSD_UCOUNTER:
				mtype = "c";
				break;
			default:
				ret = -1;
				goto cleanup;
		}

restartzero:
		// StatsD has a schizophrenic idea of what a gauge is (negative values are subtracted from previous data and not registered as is): work around its dementia
		if (zerofirst) {
			ret = snprintf(buffer, avail, "%s%s%s:0|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", pfx ? pfx : "", metrics[i].name, mtype);
			if (ret < 0) {
				ret = -1;
				goto cleanup;
			}
			else if ((size_t)ret >= avail) {
				// send what we have, reset buffer, restart - no need to add '\0': sendto will truncate anyway
				sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
				buffer = sbuffer;
				avail = STATSD_UDP_BUFSIZE;
				goto restartzero;
			}
			buffer += ret;
			avail -= (size_t)ret;
		}

restartbuffer:
		switch (metrics[i].type) {
			case STATSD_LGAUGE:
				ret = snprintf(buffer, avail, "%s%s%s:%ld|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", pfx ? pfx : "", metrics[i].name, metrics[i].value.l, mtype);
				break;
			case STATSD_UCOUNTER:
				ret = snprintf(buffer, avail, "%s%s%s:%lu|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", pfx ? pfx : "", metrics[i].name, metrics[i].value.u, mtype);
				break;
			case STATSD_FGAUGE:
				ret = snprintf(buffer, avail, "%s%s%s:%f|%s\n", statsd_runtime.namespace ? statsd_runtime.namespace : "", pfx ? pfx : "", metrics[i].name, metrics[i].value.f, mtype);
				break;
			default:
				ret = 0;
				break;	// cannot happen thanks to previous switch()
		}

		if (ret < 0) {
			ret = -1;
			goto cleanup;
		}
		else if ((size_t)ret >= avail) {
			// send what we have, reset buffer, restart - no need to add '\0': sendto will truncate anyway
			sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
			buffer = sbuffer;
			avail = STATSD_UDP_BUFSIZE;
			goto restartbuffer;
		}
		buffer += ret;
		avail -= (size_t)ret;
	}

	ret = 0;

cleanup:
	// we only check for sendto() errors here
	sent = sendto(statsd_runtime.sockfd, sbuffer, STATSD_UDP_BUFSIZE - avail, 0, (struct sockaddr *)&statsd_runtime.ai_addr, statsd_runtime.ai_addrlen);
	if (-1 == sent)
		perror(ERRPFX);

	return ret;
}


static int statsd_count(const char *pfx, const char *stat, unsigned long value)
{
	struct statsd_metric m = {
		.type = STATSD_UCOUNTER,
		.name = stat,
		.value.u = value,
	};

	return statsd_update(pfx, &m, 1);
}

static int statsd_inc(const char *pfx, const char *counter)
{
	return statsd_count(pfx, counter, 1);
}

/**
 * @param ch channel number
 * @param counter name of counter to incremetn
 */
int statsd_inc_per_channel(int ch, const char *counter)
{
	if (!statsd_runtime.namespace)
		return 0;

	char pfx[16];
	snprintf(pfx, sizeof(pfx), "%u.", R.channels[ch].Fr ? R.channels[ch].Fr : ch+1);
	return statsd_inc(pfx, counter);
}
