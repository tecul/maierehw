#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "acquisition_basic.h"
#include "core.h"

struct acquisition_basic {
    struct subscriber new_one_ms_buffer_subscriber;
    struct subscriber tracking_look_unlock_or_lock_failure_subscriber;
    uint32_t satellites;
};

struct itf_acquisition_status {
    struct itf_acquisition_sat_status sat_status[SAT_NB];
};

static void process(FLOAT complex *one_ms_buffer, uint32_t satellites, struct itf_acquisition_status *status)
{
    int i;
    int j;
    int f;
    int shift;
    struct itf_acquisition_freq_range frange = {-10000, 10000, 1000};
    struct itf_acquisition_shift_range srange = {0, 2047};

    memset(status, 0, sizeof(struct itf_acquisition_status));
    for(i = 0; i < SAT_NB; i++) {
        if (!((1 << i) & satellites))
            continue;
        detect_one_satellite(one_ms_buffer, i, 0, &frange, &srange, &status->sat_status[i]);
    }
}

static void send_msg_new_satellite_detected(int satellite_nb, struct itf_acquisition_sat_status *sat_status)
{
    struct event_satellite_detected *event_satellite_detected = (struct event_satellite_detected *) allocate_event(EVT_SATELLITE_DETECTED);

    event_satellite_detected->satellite_nb = satellite_nb;
    event_satellite_detected->doppler_freq = sat_status->doppler_freq;
    event_satellite_detected->ca_shift = sat_status->ca_shift;
    event_satellite_detected->treshold_use = sat_status->treshold_use;
    publish(&event_satellite_detected->evt);
}

static void new_one_ms_buffer_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_one_ms_buffer *event_one_ms_buffer = container_of(evt, struct event_one_ms_buffer, evt);
    struct acquisition_basic *acquisition_basic = container_of(subscriber, struct acquisition_basic, new_one_ms_buffer_subscriber);

    if (event_one_ms_buffer->first_sample_time_in_ms % 10000 == 0) {
        struct itf_acquisition_status status;
        int i;

        process(event_one_ms_buffer->file_source_buffer, acquisition_basic->satellites, &status);
        for (i = 0; i < SAT_NB; ++i)
            if (status.sat_status[i].is_present) {
                acquisition_basic->satellites &= ~(1 << i);
                send_msg_new_satellite_detected(i, &status.sat_status[i]);
            }
    }
}

static void tracking_look_unlock_or_lock_failure_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_tracking_loop_unlock_or_lock_failure *event = container_of(evt, struct event_tracking_loop_unlock_or_lock_failure, evt);
    struct acquisition_basic *acquisition_basic = container_of(subscriber, struct acquisition_basic, tracking_look_unlock_or_lock_failure_subscriber);

    acquisition_basic->satellites |= 1 << (event->satellite_nb);
}

/* public api */
handle create_acquisition_basic(uint32_t satellites)
{
    struct acquisition_basic *acquisition_basic = (struct acquisition_basic *) malloc(sizeof(struct acquisition_basic));

    assert(acquisition_basic);
    acquisition_basic->satellites = satellites;
    acquisition_basic->new_one_ms_buffer_subscriber.notify = new_one_ms_buffer_notify;
    subscribe(&acquisition_basic->new_one_ms_buffer_subscriber, EVT_ONE_MS_BUFFER);
    acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber.notify = tracking_look_unlock_or_lock_failure_notify;
    subscribe(&acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber, EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    return (handle) acquisition_basic;
}

void destroy_acquisition_basic(handle hdl)
{
    struct acquisition_basic *acquisition_basic = (struct acquisition_basic *) hdl;

    unsubscribe(&acquisition_basic->new_one_ms_buffer_subscriber, EVT_ONE_MS_BUFFER);
    unsubscribe(&acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber, EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    free(acquisition_basic);
}
