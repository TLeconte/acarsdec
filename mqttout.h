#ifndef mqttout_h
#define mqttout_h

#include <MQTTAsync.h>

typedef struct {
	MQTTAsync client;
	char *msgtopic;
} mqttout_t;

mqttout_t *MQTTinit(char *params);
void MQTTwrite(const void *buf, size_t buflen, mqttout_t *mqtt);
void MQTTexit(mqttout_t *mqtt);

#endif /* mqttout_h */
