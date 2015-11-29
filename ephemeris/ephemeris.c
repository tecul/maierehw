#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "ephemeris.h"

struct raw_eph {
    int words[5][10];
    int sf_index;
    int word[10];
    int index;
};

struct ephemeris {
    struct subscriber new_word_subscriber;
    struct raw_eph ephs[SAT_NB];
};

static inline int extract_by_word(int *word, int word_nb, int start, int size)
{
    int shift = 30 - start - size + 1;
    int mask = (1 << size) - 1;

    return (word[word_nb] >> shift) & mask;
}

static inline int extract(int *word, int start, int size)
{
    return extract_by_word(word, (start - 1) / 30, start % 30, size);
}

static void convert_from_raw_sf1(int *word, struct subframe1 *sf1)
{
    int tgd_raw = extract(word, 197, 8);
    int af0_raw = extract(word, 271, 22);
    int af1_raw = extract(word, 249, 16);
    int af2_raw = extract(word, 241, 8);

    sf1->tow = extract(word, 31, 17);
    sf1->sync_flag = extract_by_word(word, 1, 18, 1);
    sf1->momentum_flag = extract_by_word(word, 1, 19, 1);
    sf1->week_nb = extract(word, 61, 10);
    sf1->accuracy = extract(word, 73, 4);
    sf1->health = extract(word, 77, 6);
    sf1->iodc = (extract(word, 83, 2) << 8) + extract(word, 211, 8);
    sf1->tgd = ((tgd_raw << 24) >> 24) * pow(2, -31);
    sf1->toc = extract(word, 219, 16) * 16;
    sf1->af0 = ((af0_raw << 10) >> 10) * pow(2, -31);
    sf1->af1 = ((af1_raw << 16) >> 16) * pow(2, -43);
    sf1->af2 = ((af2_raw << 24) >> 24) * pow(2, -55);
}

static void convert_from_raw_sf2(int *word, struct subframe2 *sf2)
{
    int crs_raw = extract(word, 69, 16);
    int delta_n_raw = extract(word, 91, 16);
    int M0_raw = (extract(word, 107, 8) << 24) + extract(word, 121, 24);
    int Cuc_raw = extract(word, 151, 16);
    unsigned int es_raw = (extract(word, 167, 8) << 24) + extract(word, 181, 24);
    int Cus_raw = extract(word, 211, 16);
    unsigned int square_root_as_raw = (extract(word, 227, 8) << 24) + extract(word, 241, 24);

    sf2->tow = extract(word, 31, 17);
    sf2->sync_flag = extract_by_word(word, 1, 18, 1);
    sf2->momentum_flag = extract_by_word(word, 1, 19, 1);
    sf2->iode = extract(word, 61, 8);
    sf2->crs = ((crs_raw << 16) >> 16) * pow(2, -5);
    sf2->delta_n = ((delta_n_raw << 16) >> 16) * pow(2, -43) * gps_pi;
    sf2->m0 = M0_raw * pow(2, -31) * gps_pi;
    sf2->cuc = ((Cuc_raw << 16) >> 16) * pow(2, -29);
    sf2->es = es_raw * pow(2, -33);
    sf2->cus = ((Cus_raw << 16) >> 16) * pow(2, -29);
    sf2->square_root_as = square_root_as_raw * pow(2, -19);
    sf2->toe = extract(word, 271, 16) * 16;
}

static void convert_from_raw_sf3(int *word, struct subframe3 *sf3)
{
    int Cic_raw = extract(word, 61, 16);
    int omega_e_raw = (extract(word, 77, 8) << 24) + extract(word, 91, 24);
    int Cis_raw = extract(word, 121, 16);
    int io_raw = (extract(word, 137, 8) << 24) + extract(word, 151, 24);
    int Crc_raw = extract(word, 181, 16);
    int omega_raw = (extract(word, 197, 8) << 24) + extract(word, 211, 24);
    int omega_rate_raw = extract(word, 241, 24);
    int idot_raw = extract(word, 279, 14);

    sf3->tow = extract(word, 31, 17);
    sf3->sync_flag = extract_by_word(word, 1, 18, 1);
    sf3->momentum_flag = extract_by_word(word, 1, 19, 1);
    sf3->cic = ((Cic_raw << 16) >> 16) * pow(2, -29);
    sf3->omega_e = omega_e_raw * pow(2, -31) * gps_pi;
    sf3->cis = ((Cis_raw << 16) >> 16) * pow(2, -29);
    sf3->io = io_raw * pow(2, -31) * gps_pi;
    sf3->crc = ((Crc_raw << 16) >> 16) * pow(2, -5);
    sf3->omega = omega_raw * pow(2, -31) * gps_pi;
    sf3->omega_rate = ((omega_rate_raw << 8) >> 8) * pow(2, -43) * gps_pi;
    sf3->iode = extract(word, 271, 8);
    sf3->idot = ((idot_raw << 18) >> 18) * pow(2, -43) * gps_pi;
}

static void convert_from_raw(struct raw_eph *raw, struct eph *eph) {
    convert_from_raw_sf1(raw->words[0], &eph->sf1);
    convert_from_raw_sf2(raw->words[1], &eph->sf2);
    convert_from_raw_sf3(raw->words[2], &eph->sf3);
}

static void send_msg_payload_new_ephemeris(int satellite_nb, struct raw_eph *raw)
{
    struct event_ephemeris *event = (struct event_ephemeris *) allocate_event(EVT_EPHEMERIS);

    event->satellite_nb = satellite_nb;
    convert_from_raw(raw, &event->eph);
    publish(&event->evt);
}

static void new_word_notify(struct subscriber *subscriber, struct event *evt)
{
    struct event_word *event = container_of(evt, struct event_word, evt);
    struct ephemeris *ephemeris = container_of(subscriber, struct ephemeris, new_word_subscriber);
    struct raw_eph *raw = &ephemeris->ephs[event->satellite_nb];

    //printf("%2d : [%d] = 0x%08x\n", event->satellite_nb, event->index, event->word);
    if (raw->index == event->index)
        raw->word[raw->index++] = event->word;
    if (raw->index == 10) {
        int i;
        int sf_index;

        /* correct word bit value */
        for (i = 1; i < 10; i++)
            if (raw->word[i -1] & 1)
                raw->word[i] = raw->word[i] ^ 0x3fffffc0;
        /* and got sub-frame number index starting from zero */
        sf_index = ((raw->word[1] >> 8) & 7) -1 ;
        /* if it match the one we want then use it */
        if (sf_index == raw->sf_index) {
            memcpy(raw->words[raw->sf_index++], raw->word, sizeof(raw->word));
            if (raw->sf_index == 5) {
                send_msg_payload_new_ephemeris(event->satellite_nb, raw);
                raw->sf_index = 0;
            }
        }
        raw->index = 0;
    }
}

/* public api */
handle create_ephemeris()
{
    struct ephemeris *ephemeris = (struct ephemeris *) malloc(sizeof(struct ephemeris));

    assert(ephemeris);
    memset(ephemeris, 0, sizeof(struct ephemeris));
    ephemeris->new_word_subscriber.notify = new_word_notify;
    subscribe(&ephemeris->new_word_subscriber, EVT_WORD);

    return ephemeris;
}

void destroy_ephemeris(handle hdl)
{
    struct ephemeris *ephemeris = (struct ephemeris *) hdl;

    unsubscribe(&ephemeris->new_word_subscriber, EVT_WORD);

    free(ephemeris);
}
