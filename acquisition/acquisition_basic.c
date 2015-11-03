#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "acquisition_basic.h"
#include "core.h"

struct acquisition_basic {
    struct acquisition_itf acquisition_itf;
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
    struct msg_payload_new_satellite_detected msg_payload_new_satellite_detected;
    struct msg msg;

    msg.msg_type = "new_satellite_detected";
    msg.msg_payload = &msg_payload_new_satellite_detected;
    msg_payload_new_satellite_detected.satellite_nb = satellite_nb;
    msg_payload_new_satellite_detected.doppler_freq = sat_status->doppler_freq;
    msg_payload_new_satellite_detected.ca_shift = sat_status->ca_shift;
    msg_payload_new_satellite_detected.treshold_use = sat_status->treshold_use;
    publish(&msg);
}

static int init(struct acquisition_itf *acquisition_itf, uint32_t satellites)
{
    struct acquisition_basic *acquisition_basic = container_of(acquisition_itf, struct acquisition_basic, acquisition_itf);

    acquisition_basic->satellites = satellites;
}

static void destroy(struct acquisition_itf *acquisition_itf)
{
    struct acquisition_basic *acquisition_basic = container_of(acquisition_itf, struct acquisition_basic, acquisition_itf);

    unsubscribe(&acquisition_basic->new_one_ms_buffer_subscriber, "new_one_ms_buffer");
    unsubscribe(&acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber, "tracking_look_unlock_or_lock_failure");
    free(acquisition_basic);
}

static void new_one_ms_buffer_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_one_ms_buffer *msg_payload_new_one_ms_buffer = (struct msg_payload_new_one_ms_buffer *) msg->msg_payload;
    struct acquisition_basic *acquisition_basic = container_of(subscriber, struct acquisition_basic, new_one_ms_buffer_subscriber);

    if (msg_payload_new_one_ms_buffer->first_sample_time_in_ms % 10000 == 0) {
        struct itf_acquisition_status status;
        int i;

        process(msg_payload_new_one_ms_buffer->file_source_buffer, acquisition_basic->satellites, &status);
        for (i = 0; i < SAT_NB; ++i)
            if (status.sat_status[i].is_present) {
                acquisition_basic->satellites &= ~(1 << i);
                send_msg_new_satellite_detected(i, &status.sat_status[i]);
            }
    }
}

static void tracking_look_unlock_or_lock_failure_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_tracking_look_unlock_or_lock_failure *msg_payload_tracking_look_unlock_or_lock_failure = (struct msg_payload_tracking_look_unlock_or_lock_failure *) msg->msg_payload;
    struct acquisition_basic *acquisition_basic = container_of(subscriber, struct acquisition_basic, tracking_look_unlock_or_lock_failure_subscriber);

    acquisition_basic->satellites |= 1 << (msg_payload_tracking_look_unlock_or_lock_failure->satellite_nb);
}

/* public api */
struct acquisition_itf *create_acquisition_basic()
{
    struct acquisition_basic *acquisition_basic = (struct acquisition_basic *) malloc(sizeof(struct acquisition_basic));

    assert(acquisition_basic);
    acquisition_basic->acquisition_itf.init = init;
    acquisition_basic->acquisition_itf.destroy = destroy;
    acquisition_basic->satellites = 0;

    acquisition_basic->new_one_ms_buffer_subscriber.notify = new_one_ms_buffer_notify;
    subscribe(&acquisition_basic->new_one_ms_buffer_subscriber, "new_one_ms_buffer");
    acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber.notify = tracking_look_unlock_or_lock_failure_notify;
    subscribe(&acquisition_basic->tracking_look_unlock_or_lock_failure_subscriber, "tracking_look_unlock_or_lock_failure");

    return &acquisition_basic->acquisition_itf;
}

