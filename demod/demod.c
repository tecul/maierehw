#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "core.h"
#include "demod.h"

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
    struct subscriber new_pll_bit_subscriber;
    struct demod_state states[SAT_NB];
};

static enum demod_status get_first_bit_handler(struct demod_state *state, struct event_pll_bit *event)
{
    state->word = event->value;

    return DEMOD_DETECT_FIRST_TRANSITION;
}

static enum demod_status set_first_bit_handler(struct demod_state *state, struct event_pll_bit *event)
{
    state->word = event->value;
    state->first_bit_timestamp = event->timestamp;
    state->counter = 0;

    return DEMOD_RUNNING;
}

static enum demod_status detect_first_transition_handler(struct demod_state *state, struct event_pll_bit *event)
{
    state->word = (state->word << 1) + event->value;
    if (((state->word >> 1) & 1) != (state->word & 1)) {
        return set_first_bit_handler(state, event);
    }

    return DEMOD_DETECT_FIRST_TRANSITION;
}

static void send_msg_new_demod_bit(int satellite_nb, int value, double timestamp)
{
    struct event_demod_bit *event = (struct event_demod_bit *) allocate_event(EVT_DEMOD_BIT);

    event->satellite_nb = satellite_nb;
    event->value = value;
    event->timestamp = timestamp;
    publish(&event->evt);
}

static enum demod_status running_handler(struct demod_state *state, struct event_pll_bit *event)
{
    state->word = (state->word << 1) + event->value;
    if (++state->counter == 19) {
        /* we allow zero bad bit */
        if (__builtin_popcount(state->word & 0xfffff)  == 0 || __builtin_popcount(state->word & 0xfffff) == 20) {
            send_msg_new_demod_bit(event->satellite_nb, state->word & 1, state->first_bit_timestamp);
            
            return DEMOD_SET_FIRST_BIT;
        } else {
            printf("0x%08x\n", state->word);
            assert(0);
            return DEMOD_GET_FIRST_BIT;
        }
    }

    return DEMOD_RUNNING;
}

static void new_pll_bit_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_pll_bit *event = container_of(evt, struct event_pll_bit, evt);
    struct demod *demod = container_of(subscriber, struct demod, new_pll_bit_subscriber);
    struct demod_state *state = &demod->states[event->satellite_nb];

    switch(state->status) {
        case DEMOD_GET_FIRST_BIT:
            state->status = get_first_bit_handler(state, event);
            break;
        case DEMOD_DETECT_FIRST_TRANSITION:
            state->status = detect_first_transition_handler(state, event);
            break;
        case DEMOD_SET_FIRST_BIT:
            state->status = set_first_bit_handler(state, event);
            break;
        case DEMOD_RUNNING:
            state->status = running_handler(state, event);
            break;
        default:
            assert(0);
    }
}

/* public api */
handle create_demod()
{
    struct demod *demod = (struct demod *) malloc(sizeof(struct demod));

    assert(demod);
    memset(demod, 0, sizeof(struct demod));
    demod->new_pll_bit_subscriber.notify = new_pll_bit_notify;
    subscribe(&demod->new_pll_bit_subscriber, EVT_PLL_BIT);

    return demod;
}

void destroy_demod(handle hdl)
{
    struct demod *demod = (struct demod *) hdl;

    unsubscribe(&demod->new_pll_bit_subscriber, EVT_PLL_BIT);

    free(demod);
}
