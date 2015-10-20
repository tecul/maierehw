#ifndef __CORE__
#define __CORE__

#include <complex.h>
#include "gps.h"

struct msg {
    char *msg_type;
    void *msg_payload;
};

struct msg_payload_new_one_ms_buffer {
    unsigned long first_sample_time_in_ms;
    FLOAT complex file_source_buffer[GPS_SAMPLING_RATE / 1000];
};

struct msg_payload_new_satellite_detected {
	int satellite_nb;
	int doppler_freq;
	unsigned int ca_shift;
};

struct subscriber {
    void (*notify)(struct subscriber *subscriber, struct msg *msg);
};

void subscribe(struct subscriber *subscriber, char *msg_type);
void unsubscribe(struct subscriber *subscriber, char *msg_type);
void publish(struct msg *msg);

#endif
