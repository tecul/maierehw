#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "core.h"
#include "demod_impl.h"

enum demod_status {
    DEMOD_GET_FIRST_BIT,
    DEMOD_DETECT_FIRST_TRANSITION,
    DEMOD_SET_FIRST_BIT,
    DEMOD_RUNNING
};

struct demod_state {
    uint32_t word;
    enum demod_status status;
    double first_bit_timestamp;
    int counter;
};

struct demod {
    struct demod_itf demod_itf;
    struct subscriber new_pll_bit_subscriber;
    struct demod_state states[SAT_NB];
};

static enum demod_status get_first_bit_handler(struct demod_state *state, struct msg_payload_new_pll_bit *payload)
{
    state->word = payload->value;

    return DEMOD_DETECT_FIRST_TRANSITION;
}

static enum demod_status set_first_bit_handler(struct demod_state *state, struct msg_payload_new_pll_bit *payload)
{
    state->word = payload->value;
    state->first_bit_timestamp = payload->timestamp;
    state->counter = 0;

    return DEMOD_RUNNING;
}

static enum demod_status detect_first_transition_handler(struct demod_state *state, struct msg_payload_new_pll_bit *payload)
{
    state->word = (state->word << 1) + payload->value;
    if (((state->word >> 1) & 1) != (state->word & 1)) {
        return set_first_bit_handler(state, payload);
    }

    return DEMOD_DETECT_FIRST_TRANSITION;
}

static void send_msg_new_demod_bit(int satellite_nb, int value, double timestamp)
{
    struct msg_payload_new_demod_bit msg_payload_new_demod_bit;
    struct msg msg;

    msg.msg_type = "new_demod_bit";
    msg.msg_payload = &msg_payload_new_demod_bit;
    msg_payload_new_demod_bit.satellite_nb = satellite_nb;
    msg_payload_new_demod_bit.value = value;
    msg_payload_new_demod_bit.timestamp = timestamp;
    publish(&msg);
}

static enum demod_status running_handler(struct demod_state *state, struct msg_payload_new_pll_bit *payload)
{
    state->word = (state->word << 1) + payload->value;
    if (++state->counter == 19) {
        /* we allow zero bad bit */
        if (__builtin_popcount(state->word & 0xfffff)  == 0 || __builtin_popcount(state->word & 0xfffff) == 20) {
            send_msg_new_demod_bit(payload->satellite_nb, state->word & 1, state->first_bit_timestamp);
            
            return DEMOD_SET_FIRST_BIT;
        } else {
            printf("0x%08x\n", state->word);
            assert(0);
            return DEMOD_GET_FIRST_BIT;
        }
    }

    return DEMOD_RUNNING;
}

static void new_pll_bit_notify(struct subscriber *subscriber, struct msg *msg)
{
    struct msg_payload_new_pll_bit *payload = (struct msg_payload_new_pll_bit *) msg->msg_payload;
    struct demod *demod = container_of(subscriber, struct demod, new_pll_bit_subscriber);
    struct demod_state *state = &demod->states[payload->satellite_nb];

    switch(state->status) {
        case DEMOD_GET_FIRST_BIT:
            state->status = get_first_bit_handler(state, payload);
            break;
        case DEMOD_DETECT_FIRST_TRANSITION:
            state->status = detect_first_transition_handler(state, payload);
            break;
        case DEMOD_SET_FIRST_BIT:
            state->status = set_first_bit_handler(state, payload);
            break;
        case DEMOD_RUNNING:
            state->status = running_handler(state, payload);
            break;
        default:
            assert(0);
    }
}

static void destroy(struct demod_itf *demod_itf)
{
    struct demod *demod = container_of(demod_itf, struct demod, demod_itf);

    unsubscribe(&demod->new_pll_bit_subscriber, "new_pll_bit");

    free(demod);
}

/* public api */
struct demod_itf *create_demod()
{
    struct demod *demod = (struct demod *) malloc(sizeof(struct demod));

    assert(demod);
    memset(demod, 0, sizeof(struct demod));
    demod->demod_itf.destroy = destroy;
    demod->new_pll_bit_subscriber.notify = new_pll_bit_notify;
    subscribe(&demod->new_pll_bit_subscriber, "new_pll_bit");

    return &demod->demod_itf;
}
