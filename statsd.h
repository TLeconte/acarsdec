#ifndef statsd_h
#define statsd_h

typedef struct statsd_metric {
	enum { STATSD_UCOUNTER, STATSD_LGAUGE, STATSD_FGAUGE } type;
	const char *name;
	union { unsigned long u; long l; float f; } value;
} statsd_metric_t;

int statsd_init(char *params, const char *idstation);
int statsd_update(const char *pfx, const statsd_metric_t *const metrics, const unsigned int nmetrics);
int statsd_inc_per_channel(int ch, const char *counter);

#endif /* statsd_h */
