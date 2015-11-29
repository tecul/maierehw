#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "pvt.h"
#include "fix.h"

enum state {
    PVT4_WAIT_EPHEMERIS = 0,
    PVT4_RUNNING
};

struct eph_info {
    struct eph eph;
    int satellite_nb;
};

struct pvt {
    struct subscriber new_demod_bit_timestamped_subscriber;
    struct subscriber new_ephemeris_subscriber;
    int index;
    struct eph_info ephs[4];
    enum state state;
    int demod_queue_idx;
    struct event_demod_bit_timestamped demod_queue[4];
};

static void send_msg_new_pvt_raw(struct position *position)
{
    struct event_pvt_raw *event = (struct event_pvt_raw *) allocate_event(EVT_PVT_RAW);

    event->x = position->x;
    event->y = position->y;
    event->z = position->z;
    publish(&event->evt);
}

static int can_compute_pos(struct pvt *pvt)
{
    int i;

    /* check gps time is the same */
    for(i = 1; i < 4; i++) {
        if (pvt->demod_queue[0].gps_time != pvt->demod_queue[i].gps_time)
            return 0;
    }

    return 1;
}

static void compute_pos(struct pvt *pvt)
{
    struct pvt_info info;
    struct position position;
    int i;

    //printf("- compute new pos\n");
    for(i = 0; i < 4; i++) {
        ;//printf("%2d : %.15lg / %.15lg\n", pvt->demod_queue[i].satellite_nb, pvt->demod_queue[i].gps_time, pvt->demod_queue[i].timestamp);
    }
    info.gps_time = pvt->demod_queue[0].gps_time;
    for(i = 0; i < 4; i++)
        info.timestamp[i] = pvt->demod_queue[i].timestamp;
    for(i = 0; i < 4; i++) {
        int j;
        for(j = 0; j < 4; j++) {
            if (pvt->ephs[j].satellite_nb == pvt->demod_queue[i].satellite_nb) {
                info.eph[i] = pvt->ephs[j].eph;
                break;
            }
        }
        assert(j<4);
    }

    compute_fix(&info, &position);
    send_msg_new_pvt_raw(&position);
}

static int satellite_nb_ok(struct pvt *pvt, struct event_demod_bit_timestamped *event)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if (event->satellite_nb == pvt->ephs[i].satellite_nb)
            return 1;
    }

    return 0;
}

static void new_demod_bit_timestamped_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_demod_bit_timestamped *event = container_of(evt, struct event_demod_bit_timestamped, evt);
    struct pvt *pvt = container_of(subscriber, struct pvt, new_demod_bit_timestamped_subscriber);

    if (pvt->state == PVT4_RUNNING && satellite_nb_ok(pvt, event)) {
        //printf("DEMOD [%2d] : %d / %.15lg / %.15lg\n", event->satellite_nb, event->value, event->timestamp, event->gps_time);
        pvt->demod_queue[pvt->demod_queue_idx++ % 4] = *event;
        if (can_compute_pos(pvt))
            compute_pos(pvt);
    }
}

static void new_ephemeris_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_ephemeris *event = container_of(evt, struct event_ephemeris, evt);
    struct pvt *pvt = container_of(subscriber, struct pvt, new_ephemeris_subscriber);

    if (pvt->index < 4) {
        pvt->ephs[pvt->index].eph = event->eph;
        pvt->ephs[pvt->index++].satellite_nb = event->satellite_nb;
        if (pvt->index == 4)
            pvt->state = PVT4_RUNNING;
    } else {
        int i;

        for (i = 0; i < 4; i++) {
            if (pvt->ephs[i].satellite_nb == event->satellite_nb)
                pvt->ephs[i].eph = event->eph;
        }
    }
}

/* public api */
handle create_pvt_4_sat()
{
    struct pvt *pvt = (struct pvt *) malloc(sizeof(struct pvt));

    assert(pvt);
    memset(pvt, 0, sizeof(struct pvt));
    pvt->new_demod_bit_timestamped_subscriber.notify = new_demod_bit_timestamped_notify;
    subscribe(&pvt->new_demod_bit_timestamped_subscriber, EVT_DEMOD_BIT_TIMESTAMPED);
    pvt->new_ephemeris_subscriber.notify = new_ephemeris_notify;
    subscribe(&pvt->new_ephemeris_subscriber, EVT_EPHEMERIS);

    return pvt;
}

void destroy_pvt_4_sat(handle hdl)
{
    struct pvt *pvt = (struct pvt *) hdl;

    unsubscribe(&pvt->new_demod_bit_timestamped_subscriber, EVT_DEMOD_BIT_TIMESTAMPED);
    unsubscribe(&pvt->new_ephemeris_subscriber, EVT_EPHEMERIS);

    free(pvt);
}

