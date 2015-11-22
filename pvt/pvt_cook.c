#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "core.h"
#include "pvt_cook.h"
#include "fix.h"

struct pvt_cook {
    struct pvt_cook_itf pvt_cook_itf;
    struct subscriber new_pvt_raw_subscriber;
    int counter;
    double x_sum;
    double y_sum;
    double z_sum;
};

static void new_pvt_raw_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_pvt_raw *payload = (struct msg_payload_new_pvt_raw *) msg->msg_payload;
    struct pvt_cook *pvt_cook = container_of(subscriber, struct pvt_cook, new_pvt_raw_subscriber);

    pvt_cook->x_sum += payload->x;
    pvt_cook->y_sum += payload->y;
    pvt_cook->z_sum += payload->z;
    if (++pvt_cook->counter == 50) {
        struct position position;
        struct coordinate coordinate;

        position.x = pvt_cook->x_sum / 50;
        position.y = pvt_cook->y_sum / 50;
        position.z = pvt_cook->z_sum / 50;
        compute_coordinate(&position, &coordinate);
        //printf("Latitude / Longitude / altitude = %.15g %.15g %.15g\n", coordinate.latitude, coordinate.longitude, coordinate.altitude);
        printf("%.15g,%.15g,%.15g\n", coordinate.longitude, coordinate.latitude, coordinate.altitude);
        pvt_cook->x_sum = pvt_cook->y_sum = pvt_cook->z_sum = 0;
        pvt_cook->counter = 0;
    }
}

static void destroy(struct pvt_cook_itf *pvt_cook_itf)
{
    struct pvt_cook *pvt_cook = container_of(pvt_cook_itf, struct pvt_cook, pvt_cook_itf);

    unsubscribe(&pvt_cook->new_pvt_raw_subscriber, "new_pvt_raw");

    free(pvt_cook);
}

/* public api */
struct pvt_cook_itf *create_pvt_cook()
{
    struct pvt_cook *pvt_cook = (struct pvt_cook *) malloc(sizeof(struct pvt_cook));

    assert(pvt_cook);
    memset(pvt_cook, 0, sizeof(struct pvt_cook));
    pvt_cook->pvt_cook_itf.destroy = destroy;
    pvt_cook->new_pvt_raw_subscriber.notify = new_pvt_raw_notify;
    subscribe(&pvt_cook->new_pvt_raw_subscriber, "new_pvt_raw");

    return &pvt_cook->pvt_cook_itf;
}
