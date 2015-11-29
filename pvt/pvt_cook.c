#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pvt.h"
#include "fix.h"

struct pvt_cook {
    struct subscriber new_pvt_raw_subscriber;
    int counter;
    double x_sum;
    double y_sum;
    double z_sum;
};

static void new_pvt_raw_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_pvt_raw *event = container_of(evt, struct event_pvt_raw, evt);
    struct pvt_cook *pvt_cook = container_of(subscriber, struct pvt_cook, new_pvt_raw_subscriber);

    pvt_cook->x_sum += event->x;
    pvt_cook->y_sum += event->y;
    pvt_cook->z_sum += event->z;
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

/* public api */
handle create_pvt_cook()
{
    struct pvt_cook *pvt_cook = (struct pvt_cook *) malloc(sizeof(struct pvt_cook));

    assert(pvt_cook);
    memset(pvt_cook, 0, sizeof(struct pvt_cook));
    pvt_cook->new_pvt_raw_subscriber.notify = new_pvt_raw_notify;
    subscribe(&pvt_cook->new_pvt_raw_subscriber, EVT_PVT_RAW);

    return pvt_cook;
}

void destroy_pvt_cook(handle hdl)
{
    struct pvt_cook *pvt_cook = (struct pvt_cook *) hdl;

    unsubscribe(&pvt_cook->new_pvt_raw_subscriber, EVT_PVT_RAW);

    free(pvt_cook);
}
