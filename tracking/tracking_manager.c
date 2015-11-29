#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "tracking_manager.h"
#include "tracking_loop.h"
#include "core.h"

struct tracking_manager {
    struct subscriber new_satellite_detected_subscriber;
    struct subscriber tracking_look_unlock_or_lock_failure_subscriber;
    int max_channels_nb;
    int current_channels_nb;
    handle tracking_loop_handle[SAT_NB];
};

static void new_satellite_detected_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_satellite_detected *event = container_of(evt, struct event_satellite_detected, evt);
    struct tracking_manager *tracking_manager = container_of(subscriber, struct tracking_manager, new_satellite_detected_subscriber);

    //printf("satellite %d detected / doppler = %d / ca_shift = %d\n", payload->satellite_nb + 1, payload->doppler_freq, payload->ca_shift);
    if (tracking_manager->current_channels_nb < tracking_manager->max_channels_nb && !tracking_manager->tracking_loop_handle[event->satellite_nb]) {
        printf(" Create tracking loop for satellite %d detected / doppler = %d / ca_shift = %d\n", event->satellite_nb + 1, event->doppler_freq, event->ca_shift);
        tracking_manager->tracking_loop_handle[event->satellite_nb] = create_tracking_loop(event->satellite_nb);
        assert(tracking_manager->tracking_loop_handle[event->satellite_nb]);
        tracking_manager->current_channels_nb++;
    }
}

static void tracking_look_unlock_or_lock_failure_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_tracking_loop_unlock_or_lock_failure *event = container_of(evt, struct event_tracking_loop_unlock_or_lock_failure, evt);
    struct tracking_manager *tracking_manager = container_of(subscriber, struct tracking_manager, tracking_look_unlock_or_lock_failure_subscriber);

    /* let tracking loop to self destruct */
    tracking_manager->tracking_loop_handle[event->satellite_nb] = NULL;
    tracking_manager->current_channels_nb--;
}

/* public api */
handle create_tracking_manager(int max_channels_nb)
{
    struct tracking_manager *tracking_manager = (struct tracking_manager *) malloc(sizeof(struct tracking_manager));
    int i;

    assert(tracking_manager);
    tracking_manager->max_channels_nb = max_channels_nb;
    tracking_manager->current_channels_nb = 0;
    for (i = 0; i < SAT_NB; ++i)
        tracking_manager->tracking_loop_handle[i] = NULL;

    tracking_manager->new_satellite_detected_subscriber.notify = new_satellite_detected_notify;
    subscribe(&tracking_manager->new_satellite_detected_subscriber, EVT_SATELLITE_DETECTED);
    tracking_manager->tracking_look_unlock_or_lock_failure_subscriber.notify = tracking_look_unlock_or_lock_failure_notify;
    subscribe(&tracking_manager->tracking_look_unlock_or_lock_failure_subscriber, EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    return tracking_manager;
}

void destroy_tracking_manager(handle hdl)
{
    struct tracking_manager *tracking_manager = (struct tracking_manager *) hdl;
    int i;

    for (i = 0; i < SAT_NB; ++i)
        if (tracking_manager->tracking_loop_handle[i])
            destroy_tracking_loop(tracking_manager->tracking_loop_handle[i]);
    unsubscribe(&tracking_manager->new_satellite_detected_subscriber, EVT_SATELLITE_DETECTED);
    unsubscribe(&tracking_manager->tracking_look_unlock_or_lock_failure_subscriber, EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    free(tracking_manager);
}
