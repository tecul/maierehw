#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "tracking_loop.h"
#include "core.h"
#include "ca.h"

struct tl_acq_1000_hz_state {
    int counter;
};

struct tl_acq_100_hz_state {
    int doppler_freq;
    unsigned int ca_shift;
    double treshold_use;
};

struct tl_acq_10_hz_state {
    int doppler_freq;
    unsigned int ca_shift;
    double treshold_use;
};

struct tracking_loop {
    struct tracking_loop_itf tracking_loop_itf;
    struct subscriber new_one_ms_buffer_subscriber;
    int satellite_nb;
    enum tracking_loop_state state;
    union {
        struct tl_acq_1000_hz_state tl_acq_1000_hz_state;
        struct tl_acq_100_hz_state tl_acq_100_hz_state;
        struct tl_acq_10_hz_state tl_acq_10_hz_state;
        struct pll tl_pll_state;
    } ctx;
};

static void print_status(struct itf_acquisition_sat_status *sat_status)
{
#if 0
    printf("is_present = %d\n", sat_status->is_present);
    printf("doppler_freq = %d\n", sat_status->doppler_freq);
    printf("ca_shift = %d\n", sat_status->ca_shift);
    printf("treshold_use = %lg\n", sat_status->treshold_use);
#endif
}

/* fixme : factorize this */
static void send_msg_tracking_look_unlock_or_lock_failure(int satellite_nb)
{
    struct msg_payload_tracking_look_unlock_or_lock_failure msg_payload_tracking_look_unlock_or_lock_failure;
    struct msg msg;

    printf("lost in space forever for %d\n", satellite_nb + 1);
    msg.msg_type = "tracking_look_unlock_or_lock_failure";
    msg.msg_payload = &msg_payload_tracking_look_unlock_or_lock_failure;
    msg_payload_tracking_look_unlock_or_lock_failure.satellite_nb = satellite_nb;
    publish(&msg);
}

static enum tracking_loop_state tl_acq_1000_hz_state_handler(struct tracking_loop *tracking_loop, struct msg_payload_new_one_ms_buffer *payload)
{
    struct itf_acquisition_freq_range frange = {-10000, 10000, 1000};
    struct itf_acquisition_shift_range srange = {0, 2047};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(payload->file_source_buffer, tracking_loop->satellite_nb, 0, &frange,
                         &srange, &sat_status);
    print_status(&sat_status);
    if (sat_status.is_present) {
        tracking_loop->ctx.tl_acq_100_hz_state.doppler_freq = sat_status.doppler_freq;
        tracking_loop->ctx.tl_acq_100_hz_state.ca_shift = sat_status.ca_shift;
        tracking_loop->ctx.tl_acq_100_hz_state.treshold_use = sat_status.treshold_use;

        return TL_ACQ_100_HZ_STATE;
    } else if (tracking_loop->ctx.tl_acq_1000_hz_state.counter == 10) {
        send_msg_tracking_look_unlock_or_lock_failure(tracking_loop->satellite_nb);
        return TL_FAIL_STATE;
    } else
        tracking_loop->ctx.tl_acq_1000_hz_state.counter++;

    return TL_ACQ_1000_HZ_STATE;
}

static enum tracking_loop_state tl_acq_100_hz_state_handler(struct tracking_loop *tracking_loop, struct msg_payload_new_one_ms_buffer *payload)
{
    int freq = tracking_loop->ctx.tl_acq_100_hz_state.doppler_freq;
    unsigned int ca_shift = tracking_loop->ctx.tl_acq_100_hz_state.ca_shift;
    struct itf_acquisition_freq_range frange = {freq-1000, freq+1000, 100};
    struct itf_acquisition_shift_range srange = {ca_shift?ca_shift-1:ca_shift, ca_shift<2047?ca_shift + 1:ca_shift};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(payload->file_source_buffer, tracking_loop->satellite_nb, tracking_loop->ctx.tl_acq_100_hz_state.treshold_use,
                         &frange, &srange, &sat_status);
    print_status(&sat_status);
    if (sat_status.is_present) {
        tracking_loop->ctx.tl_acq_10_hz_state.doppler_freq = sat_status.doppler_freq;
        tracking_loop->ctx.tl_acq_10_hz_state.ca_shift = sat_status.ca_shift;
        tracking_loop->ctx.tl_acq_10_hz_state.treshold_use = sat_status.treshold_use;

        return TL_ACQ_10_HZ_STATE;
    }

    tracking_loop->ctx.tl_acq_1000_hz_state.counter = 0;
    return TL_ACQ_1000_HZ_STATE;
}

static enum tracking_loop_state tl_acq_10_hz_state_handler(struct tracking_loop *tracking_loop, struct msg_payload_new_one_ms_buffer *payload)
{
    int freq = tracking_loop->ctx.tl_acq_10_hz_state.doppler_freq;
    unsigned int ca_shift = tracking_loop->ctx.tl_acq_10_hz_state.ca_shift;
    struct itf_acquisition_freq_range frange = {freq-100, freq+100, 10};
    struct itf_acquisition_shift_range srange = {ca_shift?ca_shift-1:ca_shift, ca_shift<2047?ca_shift + 1:ca_shift};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(payload->file_source_buffer, tracking_loop->satellite_nb, tracking_loop->ctx.tl_acq_10_hz_state.treshold_use,
                         &frange, &srange, &sat_status);
    print_status(&sat_status);
    if (sat_status.is_present) {
        tl_init_pll_state(&tracking_loop->ctx.tl_pll_state, tracking_loop->satellite_nb, sat_status.doppler_freq, sat_status.ca_shift);

        return TL_PLL_LOCKING_STATE;
    }

    tracking_loop->ctx.tl_acq_1000_hz_state.counter = 0;
    return TL_ACQ_1000_HZ_STATE;
}

static void destroy(struct tracking_loop_itf *tracking_loop_itf)
{
    struct tracking_loop *tracking_loop = container_of(tracking_loop_itf, struct tracking_loop, tracking_loop_itf);

    unsubscribe(&tracking_loop->new_one_ms_buffer_subscriber, "new_one_ms_buffer");

    free(tracking_loop);
}

static void new_one_ms_buffer_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_one_ms_buffer *msg_payload_new_one_ms_buffer = (struct msg_payload_new_one_ms_buffer *) msg->msg_payload;
    struct tracking_loop *tracking_loop = container_of(subscriber, struct tracking_loop, new_one_ms_buffer_subscriber);

    switch(tracking_loop->state) {
        case TL_ACQ_1000_HZ_STATE:
            //printf("TL_ACQ_1000_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_1000_hz_state_handler(tracking_loop, msg->msg_payload);
            break;
        case TL_ACQ_100_HZ_STATE:
            //printf("TL_ACQ_100_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_100_hz_state_handler(tracking_loop, msg->msg_payload);
            break;
        case TL_ACQ_10_HZ_STATE:
            //printf("TL_ACQ_10_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_10_hz_state_handler(tracking_loop, msg->msg_payload);
            break;
        case TL_PLL_LOCKING_STATE:
            tracking_loop->state = tl_pll_locking_handler(&tracking_loop->ctx.tl_pll_state, msg->msg_payload);
            break;
        case TL_PLL_LOCKED_STATE:
            tracking_loop->state = tl_pll_locked_handler(&tracking_loop->ctx.tl_pll_state, msg->msg_payload);
            break;
        case TL_FAIL_STATE:
            /* auto destroy */
            //printf("lost in space forever for %d\n", tracking_loop->satellite_nb + 1);
            /* fixme it done here message broker may crash .... */
            /* need to better handle these destructions */
            break;
        default:
            assert(0);
    }
}

/* public api */
struct tracking_loop_itf *create_tracking_loop(int satellite_nb)
{
    struct tracking_loop *tracking_loop = (struct tracking_loop *) malloc(sizeof(struct tracking_loop));
    int i;

    assert(tracking_loop);
    tracking_loop->state = TL_ACQ_1000_HZ_STATE;
    tracking_loop->ctx.tl_acq_1000_hz_state.counter = 0;
    tracking_loop->satellite_nb = satellite_nb;
    tracking_loop->tracking_loop_itf.destroy = destroy;
    tracking_loop->new_one_ms_buffer_subscriber.notify = new_one_ms_buffer_notify;
    subscribe(&tracking_loop->new_one_ms_buffer_subscriber, "new_one_ms_buffer");

    return &tracking_loop->tracking_loop_itf;
}
