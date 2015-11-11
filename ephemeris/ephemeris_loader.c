#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "core.h"
#include "ephemeris_impl.h"

struct ephemeris_loader {
    struct ephemeris_loader_itf ephemeris_loader_itf;
    struct subscriber tracking_loop_lock_subscriber;
};

static void send_msg_payload_new_ephemeris(int satellite_nb)
{
    struct msg_payload_new_ephemeris msg_payload_new_ephemeris;
    struct msg msg;
    char filename[255];
    struct eph;
    int f;

    sprintf(filename, "ephemeris_%d.bin", satellite_nb);
    f = open(filename, O_RDONLY);
    if (f >= 0) {
        read(f, &msg_payload_new_ephemeris.eph, sizeof(msg_payload_new_ephemeris.eph));
        close(f);
        printf(" %s load done\n", filename);
        msg.msg_type = "new_ephemeris";
        msg.msg_payload = &msg_payload_new_ephemeris;
        msg_payload_new_ephemeris.satellite_nb = satellite_nb;
        publish(&msg);
    }
}

static void tracking_loop_lock_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_tracking_loop_lock *payload = (struct msg_payload_tracking_loop_lock *) msg->msg_payload;
    struct ephemeris_loader *loader = container_of(subscriber, struct ephemeris_loader, tracking_loop_lock_subscriber);

    send_msg_payload_new_ephemeris(payload->satellite_nb);
}

static void destroy(struct ephemeris_loader_itf *ephemeris_loader_itf)
{
    struct ephemeris_loader *ephemeris_loader = container_of(ephemeris_loader_itf, struct ephemeris_loader, ephemeris_loader_itf);

    unsubscribe(&ephemeris_loader->tracking_loop_lock_subscriber, "tracking_loop_lock");

    free(ephemeris_loader);
}

/* public api */
struct ephemeris_loader_itf *create_ephemeris_loader()
{
    struct ephemeris_loader *ephemeris_loader = (struct ephemeris_loader *) malloc(sizeof(struct ephemeris_loader));

    assert(ephemeris_loader);
    memset(ephemeris_loader, 0, sizeof(struct ephemeris_loader));
    ephemeris_loader->ephemeris_loader_itf.destroy = destroy;
    ephemeris_loader->tracking_loop_lock_subscriber.notify = tracking_loop_lock_notify;
    subscribe(&ephemeris_loader->tracking_loop_lock_subscriber, "tracking_loop_lock");

    return &ephemeris_loader->ephemeris_loader_itf;
}

