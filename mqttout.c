#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTAsync.h"


static  MQTTAsync client;
static  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
static  char *msgtopic;

int MQTTinit(char **urls, char * client_id, char *topic, char *user,char *passwd)
{
    int rc;
    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

    create_opts.maxBufferedMessages=200;
    create_opts.sendWhileDisconnected=1;
    create_opts.allowDisconnectedSendAtAnyTime=1;
    create_opts.deleteOldestMessages=1;

    MQTTAsync_createWithOptions(&client, urls[0], client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);

    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.automaticReconnect = 1;
    conn_opts.password = passwd;
    conn_opts.username = user;
    if(urls[1]) conn_opts.serverURIs = urls;

    if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        return(rc);
    }

    if(topic == NULL) {
    	msgtopic=malloc(strlen(client_id)+strlen("acarsdec")+2);
	if(msgtopic==NULL)
			return -1;
    	sprintf(msgtopic,"acarsdec/%s",client_id);
    } else
	msgtopic=topic;

    return rc;
}


int MQTTsend(char *msgtxt)
{

    pubmsg.payload = msgtxt;
    pubmsg.payloadlen = strlen(msgtxt);
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    return MQTTAsync_sendMessage(client, msgtopic, &pubmsg,NULL);

}

void MQTTend() 
{
    MQTTAsync_disconnect(client,NULL);
    MQTTAsync_destroy(&client);
}
