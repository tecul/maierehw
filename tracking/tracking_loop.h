#ifndef __TRACKING_LOOP__
#define __TRACKING_LOOP__

#include "tracking.h"
#include "gps.h"
#include "core.h"

enum tracking_loop_state {
    TL_FAIL_STATE,
    TL_ACQ_1000_HZ_STATE,
    TL_ACQ_100_HZ_STATE,
    TL_ACQ_10_HZ_STATE,
    TL_PLL_LOCKING_STATE,
    TL_PLL_LOCKED_STATE,
};

struct vco {
    double freq0;
    double freq;
    double phase;
    double mem;
};

struct dll {
	double filered_delay_error;
};

struct info {
	double filtered_lock;
	double filtered_signal_value;
	double filtered_total_power;
	int consecutive_lock_event;
	int consecutive_unlock_event;
};

struct pll {
    int sat_nb;
    struct msg_payload_new_one_ms_buffer buffer[2];
    int buffer_write_index;
    int buffer_read_index;
    int sample_read_shift;
    int sample_nb;
    struct vco vco;
    struct dll dll;
    struct info info;
};
void tl_init_pll_state(struct pll *pll, int sat_nb, int freq, unsigned int ca_shift);
enum tracking_loop_state tl_pll_locking_handler(struct pll *pll, struct msg_payload_new_one_ms_buffer *payload);
enum tracking_loop_state tl_pll_locked_handler(struct pll *pll, struct msg_payload_new_one_ms_buffer *payload);


struct tracking_loop_itf *create_tracking_loop(int satellite_nb);

#endif
