#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "core.h"
#include "ephemeris_impl.h"

struct ephemeris {
    struct ephemeris_itf ephemeris_itf;
    struct subscriber new_word_subscriber;
};

static void new_word_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_word *payload = (struct msg_payload_new_word *) msg->msg_payload;
    struct ephemeris *ephemeris = container_of(subscriber, struct ephemeris, new_word_subscriber);

    printf("%2d : [%d] = 0x%08x\n", payload->satellite_nb, payload->index, payload->word);
}

static void destroy(struct ephemeris_itf *ephemeris_itf)
{
    struct ephemeris *ephemeris = container_of(ephemeris_itf, struct ephemeris, ephemeris_itf);

    unsubscribe(&ephemeris->new_word_subscriber, "new_word");

    free(ephemeris);
}

/* public api */
struct ephemeris_itf *create_ephemeris()
{
    struct ephemeris *ephemeris = (struct ephemeris *) malloc(sizeof(struct ephemeris));

    assert(ephemeris);
    memset(ephemeris, 0, sizeof(struct ephemeris));
    ephemeris->ephemeris_itf.destroy = destroy;
    ephemeris->new_word_subscriber.notify = new_word_notify;
    subscribe(&ephemeris->new_word_subscriber, "new_word");

    return &ephemeris->ephemeris_itf;
}
