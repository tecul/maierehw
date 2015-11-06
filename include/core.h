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
    double treshold_use;
};

struct msg_payload_tracking_loop_lock {
    int satellite_nb;
};

struct msg_payload_tracking_look_unlock_or_lock_failure {
    int satellite_nb;
};

struct msg_payload_new_pll_bit {
    int satellite_nb;
    int value;
    double timestamp;
};

struct msg_payload_new_demod_bit {
    int satellite_nb;
    int value;
    double timestamp;
};

struct subscriber {
    void (*notify)(struct subscriber *subscriber, struct msg *msg);
};

struct itf_acquisition_freq_range {
    int low_freq;
    int high_freq;
    unsigned int step;
};

struct itf_acquisition_shift_range {
    int low_shift;
    int high_shift;
};

struct itf_acquisition_sat_status {
    int is_present;
    int doppler_freq;
    unsigned int ca_shift;
    double treshold_use;
};

void subscribe(struct subscriber *subscriber, char *msg_type);
void unsubscribe(struct subscriber *subscriber, char *msg_type);
void publish(struct msg *msg);
void detect_one_satellite(FLOAT complex *one_ms_buffer, int satellite_nb, double treshold_to_use, struct itf_acquisition_freq_range *frange,
                          struct itf_acquisition_shift_range *srange, struct itf_acquisition_sat_status *sat_status);

#endif
