#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"


static  MQTTClient client;
static  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
static  MQTTClient_message pubmsg = MQTTClient_message_initializer;
static  char *msgtopic;

int MQTTinit(char **urls, char * client_id, char *topic, char *user,char *passwd)
{
    int rc;

    MQTTClient_create(&client, urls[0], client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.password = passwd;
    conn_opts.username = user;
    if(urls[1]) conn_opts.serverURIs = urls;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        return(rc);
    }

    if(topic == NULL) {
    	msgtopic=malloc(strlen(client_id)+strlen("acarsdec")+2);
    	sprintf(msgtopic,"acarsdec/%s",client_id);
    } else
	msgtopic=topic;
	
    return rc;
}


int MQTTsend(char *msgtxt)
{
    MQTTClient_deliveryToken token;

    pubmsg.payload = msgtxt;
    pubmsg.payloadlen = strlen(msgtxt);
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, msgtopic, &pubmsg, &token);
    return MQTTClient_waitForCompletion(client, token, 2000);
}

void MQTTend() 
{
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}
