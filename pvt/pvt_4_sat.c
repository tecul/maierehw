#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "core.h"
#include "pvt_4_sat.h"
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
    struct pvt_itf pvt_itf;
    struct subscriber new_demod_bit_timestamped_subscriber;
    struct subscriber new_ephemeris_subscriber;
    int index;
    struct eph_info ephs[4];
    enum state state;
    int demod_queue_idx;
    struct msg_payload_new_demod_bit_timestamped demod_queue[4];
};

static void send_msg_new_pvt_raw(struct position *position)
{
    struct msg_payload_new_pvt_raw msg_payload_new_pvt_raw;
    struct msg msg;

    msg.msg_type = "new_pvt_raw";
    msg.msg_payload = &msg_payload_new_pvt_raw;
    msg_payload_new_pvt_raw.x = position->x;
    msg_payload_new_pvt_raw.y = position->y;
    msg_payload_new_pvt_raw.z = position->z;
    publish(&msg);
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

static int satellite_nb_ok(struct pvt *pvt, struct msg_payload_new_demod_bit_timestamped *payload)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if (payload->satellite_nb == pvt->ephs[i].satellite_nb)
            return 1;
    }

    return 0;
}

static void new_demod_bit_timestamped_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_demod_bit_timestamped *payload = (struct msg_payload_new_demod_bit_timestamped *) msg->msg_payload;
    struct pvt *pvt = container_of(subscriber, struct pvt, new_demod_bit_timestamped_subscriber);

    if (pvt->state == PVT4_RUNNING && satellite_nb_ok(pvt, payload)) {
        //printf("DEMOD [%2d] : %d / %.15lg / %.15lg\n", payload->satellite_nb, payload->value, payload->timestamp, payload->gps_time);
        pvt->demod_queue[pvt->demod_queue_idx++ % 4] = *payload;
        if (can_compute_pos(pvt))
            compute_pos(pvt);
    }
}

static void new_ephemeris_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_ephemeris *payload = (struct msg_payload_new_ephemeris *) msg->msg_payload;
    struct pvt *pvt = container_of(subscriber, struct pvt, new_ephemeris_subscriber);

    if (pvt->index < 4) {
        pvt->ephs[pvt->index].eph = payload->eph;
        pvt->ephs[pvt->index++].satellite_nb = payload->satellite_nb;
        if (pvt->index == 4)
            pvt->state = PVT4_RUNNING;
    } else {
        int i;

        for (i = 0; i < 4; i++) {
            if (pvt->ephs[i].satellite_nb == payload->satellite_nb)
                pvt->ephs[i].eph = payload->eph;
        }
    }
}

static void destroy(struct pvt_itf *pvt_itf)
{
    struct pvt *pvt = container_of(pvt_itf, struct pvt, pvt_itf);

    unsubscribe(&pvt->new_demod_bit_timestamped_subscriber, "new_demod_bit_timestamped");
    unsubscribe(&pvt->new_ephemeris_subscriber, "new_ephemeris");

    free(pvt);
}

/* public api */
struct pvt_itf *create_pvt_4_sat()
{
    struct pvt *pvt = (struct pvt *) malloc(sizeof(struct pvt));

    assert(pvt);
    memset(pvt, 0, sizeof(struct pvt));
    pvt->pvt_itf.destroy = destroy;
    pvt->new_demod_bit_timestamped_subscriber.notify = new_demod_bit_timestamped_notify;
    subscribe(&pvt->new_demod_bit_timestamped_subscriber, "new_demod_bit_timestamped");
    pvt->new_ephemeris_subscriber.notify = new_ephemeris_notify;
    subscribe(&pvt->new_ephemeris_subscriber, "new_ephemeris");

    return &pvt->pvt_itf;
}

