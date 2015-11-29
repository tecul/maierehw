#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "core.h"
#include "demod.h"

struct search_find_first_tlm_state {
    double gps_time;
    int word;
};

struct align_on_next_subframe_state {
    double gps_time;
    int counter;
};

struct read_and_check_tlm_state {
    double gps_time;
    int counter;
    int word;
    double demod_pos;
};

struct read_next_subframe_state {
    double gps_time;
    int tlm;
    int counter;
    int word;
    int index;
};

struct demod_word;
struct sat_state {
    void (*state_function)(struct demod_word *demod_word, struct event_demod_bit *event);
    union {
        struct search_find_first_tlm_state search_find_first_tlm_state;
        struct align_on_next_subframe_state align_on_next_subframe_state;
        struct read_and_check_tlm_state read_and_check_tlm_state;
        struct read_next_subframe_state read_next_subframe_state;
    } u;
};

struct demod_word {
    struct subscriber new_demod_bit_subscriber;
    struct sat_state sat[SAT_NB];
};

/* forward init state declaration */
static void init_search_find_first_tlm_state(struct demod_word *demod_word, int sat_nb);
static void init_align_on_next_subframe_state(struct demod_word *demod_word, int sat_nb);
static void init_read_and_check_tlm_state(struct demod_word *demod_word, int sat_nb, double gps_time);
static void init_read_next_subframe_state(struct demod_word *demod_word, int sat_nb, int tlm, double gps_time);

static void send_msg_payload_new_word(int word, int index, int satellite_nb)
{
    struct event_word *event = (struct event_word *) allocate_event(EVT_WORD);

    event->satellite_nb = satellite_nb;
    event->word = word;
    event->index = index;
    publish(&event->evt);
}

static void send_msg_payload_new_demod_bit_timestamped(int satellite_nb, int value, double timestamp, double *gps_time)
{
    if (*gps_time > 0) {
        struct event_demod_bit_timestamped *event = (struct event_demod_bit_timestamped *) allocate_event(EVT_DEMOD_BIT_TIMESTAMPED);

        event->satellite_nb = satellite_nb;
        event->value = value;
        event->timestamp = timestamp;
        event->gps_time = *gps_time;
        publish(&event->evt);
        *gps_time += 0.02;
    }
}

/* state functions */
static void search_find_first_tlm(struct demod_word *demod_word, struct event_demod_bit *event)
{
    struct search_find_first_tlm_state *state = &demod_word->sat[event->satellite_nb].u.search_find_first_tlm_state;

    state->word = (state->word << 1) + event->value;
    if ((state->word & 0x3fc00000) == 0x22c00000 || (~state->word & 0x3fc00000) == 0x22c00000)
        init_align_on_next_subframe_state(demod_word, event->satellite_nb);
}

static void align_on_next_subframe(struct demod_word *demod_word, struct event_demod_bit *event)
{
    struct align_on_next_subframe_state *state = &demod_word->sat[event->satellite_nb].u.align_on_next_subframe_state;

    if (++state->counter == 270)
        init_read_and_check_tlm_state(demod_word, event->satellite_nb, state->gps_time);
}

static void read_and_check_tlm(struct demod_word *demod_word, struct event_demod_bit *event)
{
    struct read_and_check_tlm_state *state = &demod_word->sat[event->satellite_nb].u.read_and_check_tlm_state;

    state->word = (state->word << 1) + event->value;
    if (++state->counter == 30) {
        if ((state->word & 0x3fc00000) == 0x22c00000 || (~state->word & 0x3fc00000) == 0x22c00000) {
            send_msg_payload_new_word(state->word, 0, event->satellite_nb);
            init_read_next_subframe_state(demod_word, event->satellite_nb, state->word, state->gps_time);
        } else {
            init_search_find_first_tlm_state(demod_word, event->satellite_nb);
        }
    }
}

static void read_next_subframe(struct demod_word *demod_word, struct event_demod_bit *event)
{
    struct read_next_subframe_state *state = &demod_word->sat[event->satellite_nb].u.read_next_subframe_state;

    state->word = (state->word << 1) + event->value;
    if (++state->counter == 30) {
        send_msg_payload_new_word(state->word, state->index++, event->satellite_nb);
        if (state->index == 2) {
            if (state->tlm & 1)
                state->word = state->word ^ 0x3fffffc0;
            //printf("%d : tow = %d / sf_id = %d\n", event->satellite_nb, (state->word >> 13), ((state->word >> 8) & 7));
            state->gps_time = ((state->word >> 13) - 1) * 6 + 60 * .02;
        }
        state->word = 0;
        state->counter = 0;
        if (state->index == 10)
            init_read_and_check_tlm_state(demod_word, event->satellite_nb, state->gps_time);
    }
}

/* init states api */
static void init_search_find_first_tlm_state(struct demod_word *demod_word, int sat_nb)
{
    struct search_find_first_tlm_state *state = &demod_word->sat[sat_nb].u.search_find_first_tlm_state;

    demod_word->sat[sat_nb].state_function = search_find_first_tlm;
    state->word = 0;
    state->gps_time = -1;
}

static void init_align_on_next_subframe_state(struct demod_word *demod_word, int sat_nb)
{
    struct align_on_next_subframe_state *state = &demod_word->sat[sat_nb].u.align_on_next_subframe_state;

    demod_word->sat[sat_nb].state_function = align_on_next_subframe;
    state->counter = 0;
    state->gps_time = -1;
}

static void init_read_and_check_tlm_state(struct demod_word *demod_word, int sat_nb, double gps_time)
{
    struct read_and_check_tlm_state *state = &demod_word->sat[sat_nb].u.read_and_check_tlm_state;

    demod_word->sat[sat_nb].state_function = read_and_check_tlm;
    state->counter = 0;
    state->word = 0;
    state->gps_time = gps_time;
}

static void init_read_next_subframe_state(struct demod_word *demod_word, int sat_nb, int tlm, double gps_time)
{
    struct read_next_subframe_state *state = &demod_word->sat[sat_nb].u.read_next_subframe_state;

    demod_word->sat[sat_nb].state_function = read_next_subframe;
    state->tlm = tlm;
    state->counter = 0;
    state->word = 0;
    state->index = 1;
    state->gps_time = gps_time;
}

static void new_demod_bit_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_demod_bit *event = container_of(evt, struct event_demod_bit, evt);
    struct demod_word *demod_word = container_of(subscriber, struct demod_word, new_demod_bit_subscriber);

    demod_word->sat[event->satellite_nb].state_function(demod_word, event);
    send_msg_payload_new_demod_bit_timestamped(event->satellite_nb, event->value, event->timestamp, &demod_word->sat[event->satellite_nb].u.read_next_subframe_state.gps_time);

    //printf("%15.10lg [%2d] : %d\n", payload->timestamp, payload->satellite_nb, payload->value);
}

/* public api */
handle create_demod_word()
{
    struct demod_word *demod_word = (struct demod_word *) malloc(sizeof(struct demod_word));
    int i;

    assert(demod_word);
    memset(demod_word, 0, sizeof(struct demod_word));
    demod_word->new_demod_bit_subscriber.notify = new_demod_bit_notify;
    for(i = 0; i < SAT_NB; i++)
        init_search_find_first_tlm_state(demod_word, i);
    subscribe(&demod_word->new_demod_bit_subscriber, EVT_DEMOD_BIT);

    return demod_word;
}

void destroy_demod_word(handle hdl)
{
    struct demod_word *demod_word = (struct demod_word *) hdl;

    unsubscribe(&demod_word->new_demod_bit_subscriber, EVT_DEMOD_BIT);

    free(demod_word);
}
