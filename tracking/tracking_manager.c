#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "tracking_manager.h"
#include "tracking_loop.h"
#include "core.h"

struct tracking_manager {
    struct tracking_manager_itf tracking_manager_itf;
    struct subscriber new_satellite_detected_subscriber;
    struct subscriber tracking_look_unlock_or_lock_failure_subscriber;
    int max_channels_nb;
    int current_channels_nb;
    struct tracking_loop_itf *tracking_loop_itf[SAT_NB];
};

static void destroy(struct tracking_manager_itf *tracking_manager_itf)
{
    struct tracking_manager *tracking_manager = container_of(tracking_manager_itf, struct tracking_manager, tracking_manager_itf);
    int i;

    for (i = 0; i < SAT_NB; ++i)
        if (tracking_manager->tracking_loop_itf[i])
            tracking_manager->tracking_loop_itf[i]->destroy(tracking_manager->tracking_loop_itf[i]);
    unsubscribe(&tracking_manager->new_satellite_detected_subscriber, "new_satellite_detected");

    free(tracking_manager);
}

static void new_satellite_detected_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_satellite_detected *payload = (struct msg_payload_new_satellite_detected *) msg->msg_payload;
    struct tracking_manager *tracking_manager = container_of(subscriber, struct tracking_manager, new_satellite_detected_subscriber);

    //printf("satellite %d detected / doppler = %d / ca_shift = %d\n", payload->satellite_nb + 1, payload->doppler_freq, payload->ca_shift);
    if (tracking_manager->current_channels_nb < tracking_manager->max_channels_nb && !tracking_manager->tracking_loop_itf[payload->satellite_nb]) {
        printf(" Create tracking loop for satellite %d detected / doppler = %d / ca_shift = %d\n", payload->satellite_nb + 1, payload->doppler_freq, payload->ca_shift);
        tracking_manager->tracking_loop_itf[payload->satellite_nb] = create_tracking_loop(payload->satellite_nb);
        assert(tracking_manager->tracking_loop_itf[payload->satellite_nb]);
        tracking_manager->current_channels_nb++;
    }
}

static void tracking_look_unlock_or_lock_failure_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_tracking_look_unlock_or_lock_failure *payload = (struct msg_payload_tracking_look_unlock_or_lock_failure *) msg->msg_payload;
    struct tracking_manager *tracking_manager = container_of(subscriber, struct tracking_manager, tracking_look_unlock_or_lock_failure_subscriber);

    /* let tracking loop to self destruct */
    tracking_manager->tracking_loop_itf[payload->satellite_nb] = NULL;
    tracking_manager->current_channels_nb--;
}

/* public api */
struct tracking_manager_itf *create_tracking_manager(int max_channels_nb)
{
    struct tracking_manager *tracking_manager = (struct tracking_manager *) malloc(sizeof(struct tracking_manager));
    int i;

    assert(tracking_manager);
    tracking_manager->tracking_manager_itf.destroy = destroy;
    tracking_manager->max_channels_nb = max_channels_nb;
    tracking_manager->current_channels_nb = 0;
    for (i = 0; i < SAT_NB; ++i)
        tracking_manager->tracking_loop_itf[i] = NULL;

    tracking_manager->new_satellite_detected_subscriber.notify = new_satellite_detected_notify;
    subscribe(&tracking_manager->new_satellite_detected_subscriber, "new_satellite_detected");
    tracking_manager->tracking_look_unlock_or_lock_failure_subscriber.notify = tracking_look_unlock_or_lock_failure_notify;
    subscribe(&tracking_manager->tracking_look_unlock_or_lock_failure_subscriber, "tracking_look_unlock_or_lock_failure_subscriber");

    return &tracking_manager->tracking_manager_itf;
}
