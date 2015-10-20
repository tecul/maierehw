#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "acquisition_basic.h"
#include "ca.h"
#include "core.h"

struct acquisition_basic {
    struct acquisition_itf acquisition_itf;
    struct subscriber new_one_ms_buffer_subscriber;
    uint32_t satellites;
};

struct itf_acquisition_sat_status {
    int is_present;
    int doppler_freq;
    unsigned int ca_shift;
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

    memset(status, 0, sizeof(struct itf_acquisition_status));
    for(i = 0; i < SAT_NB; i++) {
        double max = 0;
        unsigned int ca_shift = 0;
        int doppler_freq = 0;
        int sum_value = 0;
        int sum_number = 0;

        if (!((1 << i) & satellites))
            continue;
        for(f = -10000; f <= 10000; f+= 1000) {
            FLOAT complex doppler_samples[2048];
            FLOAT complex samples_mixed[2048];
            double step = 1.0 / 2048000.0;
            double w = 2 * M_PI * f;
            double max_shifted = 0;
            int max_shifted_shift_value = -1;

            /* build doppler samples */
            for(j = 0; j < 2048; j++)
                doppler_samples[j] = 127.0 * cos(w * j * step) + 127.0 * sin(w * j * step) * I;
            /* mix samples so we remove doppler effect */
            for(j = 0; j < 2048; j++)
                samples_mixed[j] = one_ms_buffer[j] * doppler_samples[j];
            /* now mix with ca shifted and compute sum */
            for(shift = 0; shift < 2048; shift++) {
                FLOAT complex corr = 0;
                for(j = 0; j < 2048; j++)
                    corr += samples_mixed[j] * ca[i][j + shift];
                if (cabs(corr) > max_shifted) {
                    max_shifted = cabs(corr);
                    max_shifted_shift_value = shift;
                }
            }
            /* do we have a new max */
            sum_value += max_shifted;
            sum_number++;
            if (max_shifted > max) {
                max = max_shifted;
                ca_shift = max_shifted_shift_value;
                doppler_freq = f;
            }
        }
        /* do we have a sat */
        if (max > 1.5 * sum_value / sum_number) {
            status->sat_status[i].is_present = 1;
            status->sat_status[i].doppler_freq = doppler_freq;
            status->sat_status[i].ca_shift = ca_shift;
        }
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
    free(acquisition_basic);
}

static void new_one_ms_buffer_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_one_ms_buffer *msg_payload_new_one_ms_buffer = (struct msg_payload_new_one_ms_buffer *) msg->msg_payload;
    struct acquisition_basic *acquisition_basic = container_of(subscriber, struct acquisition_basic, new_one_ms_buffer_subscriber);

    if (msg_payload_new_one_ms_buffer->first_sample_time_in_ms % 1000 == 0) {
        struct itf_acquisition_status status;
        int i;

        process(msg_payload_new_one_ms_buffer->file_source_buffer, acquisition_basic->satellites, &status);
        for (i = 0; i < SAT_NB; ++i)
            if (status.sat_status[i].is_present)
                send_msg_new_satellite_detected(i, &status.sat_status[i]);
    }
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

    return &acquisition_basic->acquisition_itf;
}

