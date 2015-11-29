#ifndef __CORE__
#define __CORE__

#include <complex.h>
#include <libr/list.h>
#include <stdint.h>
#include "gps.h"

typedef void * handle;

typedef enum event_type {
    EVT_QUEUE_EMPTY,
    EVT_EXIT,
    EVT_ONE_MS_BUFFER,
    EVT_SATELLITE_DETECTED,
    EVT_TRACKING_LOOP_LOCK,
    EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE,
    EVT_PLL_BIT,
    EVT_DEMOD_BIT,
    EVT_DEMOD_BIT_TIMESTAMPED,
    EVT_WORD,
    EVT_EPHEMERIS,
    EVT_NB
} event_type_t;

struct event {
    enum event_type type;
    int size;
    struct list_head list;
};

struct event_queue_empty {
    struct event evt;
};

struct event_exit {
    struct event evt;
};

struct event_one_ms_buffer {
    struct event evt;
    unsigned long first_sample_time_in_ms;
    FLOAT complex file_source_buffer[GPS_SAMPLING_RATE / 1000];
};

struct event_satellite_detected {
    struct event evt;
    int satellite_nb;
    int doppler_freq;
    unsigned int ca_shift;
    double treshold_use;
};

struct event_tracking_loop_lock {
    struct event evt;
    int satellite_nb;
};

struct event_tracking_loop_unlock_or_lock_failure {
    struct event evt;
    int satellite_nb;
};

struct event_pll_bit {
    struct event evt;
    int satellite_nb;
    int value;
    double timestamp;
};

struct event_demod_bit {
    struct event evt;
    int satellite_nb;
    int value;
    double timestamp;
};

struct event_demod_bit_timestamped {
    struct event evt;
    int satellite_nb;
    int value;
    double timestamp;
    double gps_time;
};

struct event_word {
    struct event evt;
    int satellite_nb;
    int word;
    int index;
};

struct event_ephemeris {
    struct event evt;
    int satellite_nb;
    struct eph eph;
};

struct subscriber {
    struct list_head list;
    void (*notify)(struct subscriber *subscriber, struct event *evt);
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

struct event *allocate_event(event_type_t event_type);
void deallocate_event(struct event *evt);
void init_event_module();
void subscribe(struct subscriber *subscriber, event_type_t event_type);
void unsubscribe(struct subscriber *subscriber, event_type_t event_type);
void publish(struct event *evt);
void event_loop();

void detect_one_satellite(FLOAT complex *one_ms_buffer, int satellite_nb, double treshold_to_use, struct itf_acquisition_freq_range *frange,
                          struct itf_acquisition_shift_range *srange, struct itf_acquisition_sat_status *sat_status);



#endif
