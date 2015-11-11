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

struct ephemeris_dumper {
    struct ephemeris_dumper_itf ephemeris_dumper_itf;
    struct subscriber new_ephemeris_subscriber;
    int ephs_status[SAT_NB];
};

static void dump(struct msg_payload_new_ephemeris *payload)
{
    char filename[255];
    int f;

    sprintf(filename, "ephemeris_%d.bin", payload->satellite_nb);
    f = open(filename, O_WRONLY | O_CREAT, S_IRWXU);
    if (f >= 0) {
        write(f, &payload->eph, sizeof(payload->eph));
        close(f);
        printf(" %s dump done\n", filename);
    }
}

static void new_ephemeris_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_ephemeris *payload = (struct msg_payload_new_ephemeris *) msg->msg_payload;
    struct ephemeris_dumper *dumper = container_of(subscriber, struct ephemeris_dumper, new_ephemeris_subscriber);

    printf("Got new new_ephemeris for sat %d\n", payload->satellite_nb);
    if (dumper->ephs_status[payload->satellite_nb] == 0) {
        dump(payload);
        dumper->ephs_status[payload->satellite_nb] = 1;
    }
}

static void destroy(struct ephemeris_dumper_itf *ephemeris_dumper_itf)
{
    struct ephemeris_dumper *ephemeris_dumper = container_of(ephemeris_dumper_itf, struct ephemeris_dumper, ephemeris_dumper_itf);

    unsubscribe(&ephemeris_dumper->new_ephemeris_subscriber, "new_ephemeris");

    free(ephemeris_dumper);
}

/* public api */
struct ephemeris_dumper_itf *create_ephemeris_dumper()
{
    struct ephemeris_dumper *ephemeris_dumper = (struct ephemeris_dumper *) malloc(sizeof(struct ephemeris_dumper));

    assert(ephemeris_dumper);
    memset(ephemeris_dumper, 0, sizeof(struct ephemeris_dumper));
    ephemeris_dumper->ephemeris_dumper_itf.destroy = destroy;
    ephemeris_dumper->new_ephemeris_subscriber.notify = new_ephemeris_notify;
    subscribe(&ephemeris_dumper->new_ephemeris_subscriber, "new_ephemeris");

    return &ephemeris_dumper->ephemeris_dumper_itf;
}
