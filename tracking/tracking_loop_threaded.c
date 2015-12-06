#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>

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
    struct subscriber new_one_ms_buffer_subscriber;
    int satellite_nb;
    enum tracking_loop_state state;
    union {
        struct tl_acq_1000_hz_state tl_acq_1000_hz_state;
        struct tl_acq_100_hz_state tl_acq_100_hz_state;
        struct tl_acq_10_hz_state tl_acq_10_hz_state;
        struct pll tl_pll_state;
    } ctx;
    pthread_t worker;
    sem_t thread_idle_sem;
    sem_t event_ready_sem;
    volatile int thread_exit;
    struct event_one_ms_buffer *event_one_ms_buffer;
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
    struct event_tracking_loop_unlock_or_lock_failure *event = (struct event_tracking_loop_unlock_or_lock_failure *) allocate_event(EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    printf("loose lock or lock failure for satellite %d\n", satellite_nb + 1);
    event->satellite_nb = satellite_nb;
    publish(&event->evt);
}

static enum tracking_loop_state tl_acq_1000_hz_state_handler(struct tracking_loop *tracking_loop, struct event_one_ms_buffer *evt)
{
    struct itf_acquisition_freq_range frange = {-10000, 10000, 1000};
    struct itf_acquisition_shift_range srange = {0, 2047};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(evt->file_source_buffer, tracking_loop->satellite_nb, 0, &frange,
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

static enum tracking_loop_state tl_acq_100_hz_state_handler(struct tracking_loop *tracking_loop, struct event_one_ms_buffer *evt)
{
    int freq = tracking_loop->ctx.tl_acq_100_hz_state.doppler_freq;
    unsigned int ca_shift = tracking_loop->ctx.tl_acq_100_hz_state.ca_shift;
    struct itf_acquisition_freq_range frange = {freq-1000, freq+1000, 100};
    struct itf_acquisition_shift_range srange = {ca_shift?ca_shift-1:ca_shift, ca_shift<2047?ca_shift + 1:ca_shift};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(evt->file_source_buffer, tracking_loop->satellite_nb, tracking_loop->ctx.tl_acq_100_hz_state.treshold_use,
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

static enum tracking_loop_state tl_acq_10_hz_state_handler(struct tracking_loop *tracking_loop, struct event_one_ms_buffer *evt)
{
    int freq = tracking_loop->ctx.tl_acq_10_hz_state.doppler_freq;
    unsigned int ca_shift = tracking_loop->ctx.tl_acq_10_hz_state.ca_shift;
    struct itf_acquisition_freq_range frange = {freq-100, freq+100, 10};
    struct itf_acquisition_shift_range srange = {ca_shift?ca_shift-1:ca_shift, ca_shift<2047?ca_shift + 1:ca_shift};
    struct itf_acquisition_sat_status sat_status;

    detect_one_satellite(evt->file_source_buffer, tracking_loop->satellite_nb, tracking_loop->ctx.tl_acq_10_hz_state.treshold_use,
                         &frange, &srange, &sat_status);
    print_status(&sat_status);
    if (sat_status.is_present) {
        tl_init_pll_state(&tracking_loop->ctx.tl_pll_state, tracking_loop->satellite_nb, sat_status.doppler_freq, sat_status.ca_shift);

        return TL_PLL_LOCKING_STATE;
    }

    tracking_loop->ctx.tl_acq_1000_hz_state.counter = 0;
    return TL_ACQ_1000_HZ_STATE;
}

static void process_event(struct tracking_loop *tracking_loop, struct event *evt)
{
    struct event_one_ms_buffer *event_one_ms_buffer = container_of(evt, struct event_one_ms_buffer, evt);

    switch(tracking_loop->state) {
        case TL_ACQ_1000_HZ_STATE:
            //printf("TL_ACQ_1000_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_1000_hz_state_handler(tracking_loop, event_one_ms_buffer);
            break;
        case TL_ACQ_100_HZ_STATE:
            //printf("TL_ACQ_100_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_100_hz_state_handler(tracking_loop, event_one_ms_buffer);
            break;
        case TL_ACQ_10_HZ_STATE:
            //printf("TL_ACQ_10_HZ_STATE %d\n", tracking_loop->satellite_nb);
            tracking_loop->state = tl_acq_10_hz_state_handler(tracking_loop, event_one_ms_buffer);
            break;
        case TL_PLL_LOCKING_STATE:
            tracking_loop->state = tl_pll_locking_handler(&tracking_loop->ctx.tl_pll_state, event_one_ms_buffer);
            break;
        case TL_PLL_LOCKED_STATE:
            tracking_loop->state = tl_pll_locked_handler(&tracking_loop->ctx.tl_pll_state, event_one_ms_buffer);
            break;
        case TL_FAIL_STATE:
            /* waiting for being destroy */
            break;
        default:
            assert(0);
    }
}

static void *new_one_ms_buffer_notify_worker_thread(void *arg)
{
    struct tracking_loop *tracking_loop = (struct tracking_loop *) arg;

    //printf("Thread for %d starting\n", tracking_loop->satellite_nb);
    sem_post(&tracking_loop->thread_idle_sem);
    while(1) {
        sem_wait(&tracking_loop->event_ready_sem);
        if (tracking_loop->thread_exit)
            break;
        process_event(tracking_loop, &tracking_loop->event_one_ms_buffer->evt);
        deallocate_event(&tracking_loop->event_one_ms_buffer->evt);
        sem_post(&tracking_loop->thread_idle_sem);
    }
    //printf("Thread for %d exiting\n", tracking_loop->satellite_nb);

    return NULL;
}

static void new_one_ms_buffer_notify(struct subscriber *subscriber, struct event *evt)
{
    struct tracking_loop *tracking_loop = container_of(subscriber, struct tracking_loop, new_one_ms_buffer_subscriber);

    sem_wait(&tracking_loop->thread_idle_sem);
    tracking_loop->event_one_ms_buffer = (struct event_one_ms_buffer *) duplicate_event(evt);
    sem_post(&tracking_loop->event_ready_sem);
}

/* public api */
handle create_tracking_loop_threaded(int satellite_nb)
{
    struct tracking_loop *tracking_loop = (struct tracking_loop *) malloc(sizeof(struct tracking_loop));
    int i;
    int res;

    assert(tracking_loop);
    tracking_loop->state = TL_ACQ_1000_HZ_STATE;
    tracking_loop->ctx.tl_acq_1000_hz_state.counter = 0;
    tracking_loop->satellite_nb = satellite_nb;
    tracking_loop->new_one_ms_buffer_subscriber.notify = new_one_ms_buffer_notify;
    tracking_loop->thread_exit = 0;
    res = sem_init(&tracking_loop->thread_idle_sem, 0, 0);
    assert(res == 0);
    res = sem_init(&tracking_loop->event_ready_sem, 0, 0);
    assert(res == 0);
    res = pthread_create(&tracking_loop->worker, NULL, new_one_ms_buffer_notify_worker_thread, tracking_loop);
    assert(res == 0);
    subscribe(&tracking_loop->new_one_ms_buffer_subscriber, EVT_ONE_MS_BUFFER);

    return tracking_loop;
}

void destroy_tracking_loop_threaded(handle hdl)
{
    struct tracking_loop *tracking_loop = (struct tracking_loop *) hdl;

    unsubscribe(&tracking_loop->new_one_ms_buffer_subscriber, EVT_ONE_MS_BUFFER);
    tracking_loop->thread_exit = 1;
    sem_post(&tracking_loop->event_ready_sem);
    pthread_join(tracking_loop->worker, NULL);

    free(tracking_loop);
}
