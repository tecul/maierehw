#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "core.h"
#include "pvt_4_sat.h"

struct pvt {
    struct pvt_itf pvt_itf;
    struct subscriber new_demod_bit_timestamped_subscriber;
    struct subscriber new_ephemeris_subscriber;
};

static void destroy(struct pvt_itf *pvt_itf)
{
    struct pvt *pvt = container_of(pvt_itf, struct pvt, pvt_itf);

    unsubscribe(&pvt->new_demod_bit_timestamped_subscriber, "new_demod_bit_timestamped");
    unsubscribe(&pvt->new_ephemeris_subscriber, "new_ephemeris");

    free(pvt);
}

static void new_demod_bit_timestamped_notify(struct subscriber *subscriber, struct msg *msg)
{
    ;
}

static void new_ephemeris_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_ephemeris *payload = (struct msg_payload_new_ephemeris *) msg->msg_payload;
    struct pvt *pvt = container_of(subscriber, struct pvt, new_ephemeris_subscriber);

    printf("Got new new_ephemeris for sat %d\n", payload->satellite_nb);
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

