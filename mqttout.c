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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTAsync.h>

#include "acarsdec.h"
#include "mqttout.h"

#define ERRPFX	"ERROR: MQTT: "
#define WARNPFX	"WARNING: MQTT: "

// params is "uri=protocol://host:port,uri=protocol://host:port,user=username,passwd=password,topic=mytopic
mqttout_t *MQTTinit(char *params)
{
	mqttout_t *mqpriv;
	char *urls[15] = {};
	char **url, *topic = NULL, *user = NULL, *passwd = NULL, *msgtopic = NULL;
	char *param, *sep;
	int rc;
	MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

	url = urls;
	while ((param = strsep(&params, ","))) {
		sep = strchr(param, '=');
		if (!sep)
			continue;
		*sep++ = '\0';
		if (!strcmp("topic", param))
			topic = sep;
		else if (!strcmp("user", param))
			user = sep;
		else if (!strcmp("passwd", param))
			passwd = sep;
		else if (!strcmp("uri", param)) {
			if (url > &urls[14])
				fprintf(stderr, WARNPFX "too many urls provided, ignoring '%s'\n", sep);
			else
				*url++ = sep;
		}
	}

	if (!urls[0]) {
		fprintf(stderr, ERRPFX "no URI provided\n");
		return NULL;
	}

	create_opts.maxBufferedMessages = 200;
	create_opts.sendWhileDisconnected = 1;
	create_opts.allowDisconnectedSendAtAnyTime = 1;
	create_opts.deleteOldestMessages = 1;

	mqpriv = calloc(1, sizeof(*mqpriv));
	if (!mqpriv) {
		perror(NULL);
		return NULL;
	}

	MQTTAsync_createWithOptions(&mqpriv->client, urls[0], R.idstation, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);

	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.automaticReconnect = 1;
	conn_opts.password = passwd;
	conn_opts.username = user;
	if (urls[1])
		conn_opts.serverURIs = urls;

	if ((rc = MQTTAsync_connect(mqpriv->client, &conn_opts)) != MQTTASYNC_SUCCESS) {
		fprintf(stderr, ERRPFX "failed to connect\n");
		goto fail;
	}

	if (topic == NULL) {
		msgtopic = malloc(strlen(R.idstation) + strlen("acarsdec") + 2);
		if (msgtopic == NULL) {
			perror(NULL);
			goto fail;
		}
		sprintf(msgtopic, "acarsdec/%s", R.idstation);
	} else
		msgtopic = strdup(topic);
	mqpriv->msgtopic = msgtopic;

	return mqpriv;

fail:
	free(mqpriv);
	return NULL;
}

void MQTTwrite(const void *buf, size_t buflen, mqttout_t *mqtt)
{
	if (MQTTAsync_send(mqtt->client, mqtt->msgtopic, buflen, buf, 0, 0, NULL) != MQTTASYNC_SUCCESS)
		fprintf(stderr, WARNPFX "failed to send, ignoring.\n");
}

void MQTTexit(mqttout_t *mqtt)
{
	MQTTAsync_disconnect(mqtt->client, NULL);
	MQTTAsync_destroy(&mqtt->client);
	free(mqtt->msgtopic);
	free(mqtt);
}
