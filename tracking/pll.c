#include <math.h>
#include <string.h>
#include <stdio.h>

#include "tracking_loop.h"
#include "ca.h"

static double lock_detector(double phi_error, double *filtered_lock) {

    *filtered_lock = 0.9 * (*filtered_lock) + 0.1 * cos(2 * phi_error);

    return fabs(*filtered_lock);
}

static double snr(FLOAT complex *xcorr, double *signal_value, double *total_power)
{
    *signal_value = 0.9 * (*signal_value) + 0.1 * fabs(creal(*xcorr));
    *total_power  = 0.9 * (*total_power)  + 0.1 * (creal(*xcorr) * creal(*xcorr) + cimag(*xcorr) * cimag(*xcorr));

    double signal_power = (*signal_value) * (*signal_value);
    return 10 * log10(signal_power / (*total_power - signal_power)) + 10 * log10((double)GPS_SAMPLING_RATE / 2) - 10 * log10(1023.0);
}

static double complex corrr(int nb, FLOAT complex *buf, double freq, double *phase)
{
    double step = 1.0 / 2048000.0;
    double w = 2 * M_PI * freq;
    double complex res = 0;
    int j;

    for(j = 0; j < 2048; j++)
        res += buf[j] * ca[nb][j] * (127.0 * cos(*phase + w * j * step) + 127.0 * sin(*phase + w * j * step) * I);

    *phase = *phase + w * 2048 * step;

    return res;
}

static double complex early(int nb, FLOAT complex *buf, double freq, double phase)
{
    double step = 1.0 / 2048000.0;
    double w = 2 * M_PI * freq;
    double complex res = 0;
    int j;

    for(j = 0; j < 2048; j++)
        res += buf[j] * ca[nb][j + 2047] * (127.0 * cos(phase + w * j * step) + 127.0 * sin(phase + w * j * step) * I);

    return res;
}

static double complex late(int nb, FLOAT complex *buf, double freq, double phase)
{
    double step = 1.0 / 2048000.0;
    double w = 2 * M_PI * freq;
    double complex res = 0;
    int j;

    for(j = 0; j < 2048; j++)
        res += buf[j] * ca[nb][j + 1] * (127.0 * cos(phase + w * j * step) + 127.0 * sin(phase + w * j * step) * I);

    return res;
}

static double delay_error(FLOAT complex early, FLOAT complex prompt, FLOAT complex late)
{
    double E = cabs(early);
    double L = cabs(late);

    return 0.5 * (E - L) / (E + L);
}

static double filter(double in, double *mem)
{
    const double c1 = 185.757;
    const double c2 = 6.19188;
    
    *mem = in * c2 + *mem;
    return in * c1 + *mem;
}

static void pll_add_buffer(struct pll *pll, struct event_one_ms_buffer *evt)
{
    pll->buffer[pll->buffer_write_index] = *evt;
    pll->buffer_write_index = 1 - pll->buffer_write_index;
    pll->sample_nb += SAMPLE_NB_PER_MS;
}

static double pll_build_buffer(struct pll *pll, FLOAT complex *buf)
{
    double time_stamp = pll->buffer[pll->buffer_read_index].first_sample_time_in_ms * 0.001 + (double)pll->sample_read_shift / 2048000.0;

    memcpy(buf, &pll->buffer[pll->buffer_read_index].file_source_buffer[pll->sample_read_shift], (SAMPLE_NB_PER_MS - pll->sample_read_shift) * sizeof(FLOAT complex));
    pll->buffer_read_index = 1 - pll->buffer_read_index;
    if (pll->sample_read_shift)
        memcpy(&buf[SAMPLE_NB_PER_MS - pll->sample_read_shift], &pll->buffer[pll->buffer_read_index].file_source_buffer[0], pll->sample_read_shift * sizeof(FLOAT complex));
    pll->sample_nb -= SAMPLE_NB_PER_MS;

    return time_stamp;
}

static int pll_compute_bit(struct pll *pll, FLOAT complex *buf, double *time_stamp)
{
    double complex xearly = early(pll->sat_nb, buf, pll->vco.freq, pll->vco.phase);
    double complex xlate = late(pll->sat_nb, buf, pll->vco.freq, pll->vco.phase);
    double complex xcorr = corrr(pll->sat_nb, buf, pll->vco.freq, &pll->vco.phase);
    double phi_error = atan(cimag(xcorr) / creal(xcorr));
    double vco_command = filter(phi_error, &pll->vco.mem);
    double lock_value = lock_detector(phi_error, &pll->info.filtered_lock);
    double snr_value = snr(&xcorr, &pll->info.filtered_signal_value, &pll->info.filtered_total_power);
    int is_lock = lock_value > 0.75?1:0;
    int delay = 0;

    /* correct timestamp */
    *time_stamp += pll->dll.filered_delay_error * 2 / 2048000.0;
    /* update vco */
    pll->vco.freq = pll->vco.freq0 - vco_command * .25 / 2 / M_PI;
    /* dll part */
    pll->dll.filered_delay_error = 0.9 * pll->dll.filered_delay_error + 0.1 * delay_error(xearly, xcorr, xlate);
    if (pll->dll.filered_delay_error < -0.3) {
        pll->dll.filered_delay_error += 0.5;
        delay = 1;
    } else if (pll->dll.filered_delay_error > 0.3) {
        pll->dll.filered_delay_error -= 0.5;
        delay = -1;
    }
    if (delay) {
        pll->sample_read_shift = pll->sample_read_shift - delay;
        pll->sample_nb = pll->sample_nb + delay;
        if (pll->sample_read_shift < 0 || pll->sample_read_shift > 2047) {
            pll->sample_read_shift = (pll->sample_read_shift + 2048) % 2048;
            pll->buffer_read_index = 1 - pll->buffer_read_index;
        }
    }

    /* lock counter update */
    pll->info.consecutive_lock_event = is_lock?pll->info.consecutive_lock_event+1:0;
    pll->info.consecutive_unlock_event = is_lock?0:pll->info.consecutive_unlock_event+1;

    //printf("%.15lg ms | %lg|%lg|%lg : (%g+j%g)[%g] | l = %lg / %d/%d | snr = %lg | dll_error = %lg\n", *time_stamp, phi_error, vco_command, pll->vco.freq, creal(xcorr) / cabs(xcorr), cimag(xcorr) / cabs(xcorr), cabs(xcorr),
    //                                                      lock_value, lock_value > 0.75?1:0, pll->info.consecutive_lock_event, snr_value, pll->dll.filered_delay_error);
    //printf(" %lg > %lg > %lg => %lg\n", cabs(xearly), cabs(xcorr), cabs(xlate), pll->dll.filered_delay_error);

    return (acos(creal(xcorr) / cabs(xcorr)) >= M_PI/2)?1:0;
}

static void send_msg_tracking_loop_lock(int satellite_nb, double timestamp)
{
    struct event_tracking_loop_lock *event = (struct event_tracking_loop_lock *) allocate_event(EVT_TRACKING_LOOP_LOCK);

    printf("sat_nb %d locked (%lg ms) !!!!\n", satellite_nb + 1, timestamp);
    event->satellite_nb = satellite_nb;
    publish(&event->evt);
}

static void send_msg_tracking_look_unlock_or_lock_failure(int satellite_nb)
{
    struct event_tracking_loop_unlock_or_lock_failure *event = (struct event_tracking_loop_unlock_or_lock_failure *) allocate_event(EVT_TRACKING_LOOP_UNLOCK_OR_LOCK_FAILURE);

    printf("loose lock or lock failure for satellite %d\n", satellite_nb + 1);
    event->satellite_nb = satellite_nb;
    publish(&event->evt);
}

static void send_msg_new_pll_bit(int satellite_nb, int value, double timestamp)
{
    struct event_pll_bit *event = (struct event_pll_bit *) allocate_event(EVT_PLL_BIT);

    event->satellite_nb = satellite_nb;
    event->value = value;
    event->timestamp = timestamp;
    publish(&event->evt);
}

/* public api */
void tl_init_pll_state(struct pll *pll, int sat_nb, int freq, unsigned int ca_shift)
{
    memset(pll, 0, sizeof(struct pll));
    pll->sat_nb = sat_nb;
    pll->sample_read_shift = ca_shift?SAMPLE_NB_PER_MS - ca_shift:SAMPLE_NB_PER_MS;
    pll->sample_nb = ca_shift?-ca_shift:0;
    pll->vco.freq0 = freq;
    pll->vco.freq = freq;
}

enum tracking_loop_state tl_pll_locking_handler(struct pll *pll, struct event_one_ms_buffer *evt)
{
    double time_stamp;

    pll_add_buffer(pll, evt);

    while (pll->sample_nb >= SAMPLE_NB_PER_MS) {
        FLOAT complex buf[SAMPLE_NB_PER_MS];

        time_stamp = pll_build_buffer(pll, buf);
        pll_compute_bit(pll, buf, &time_stamp);
    }

    if (pll->info.consecutive_lock_event > 32) {
        send_msg_tracking_loop_lock(pll->sat_nb, time_stamp);
        return TL_PLL_LOCKED_STATE;
    }

    if (pll->info.consecutive_unlock_event > 10000) {
        send_msg_tracking_look_unlock_or_lock_failure(pll->sat_nb);
        return TL_FAIL_STATE;
    }

    return TL_PLL_LOCKING_STATE;
}

enum tracking_loop_state tl_pll_locked_handler(struct pll *pll, struct event_one_ms_buffer *evt)
{
    pll_add_buffer(pll, evt);
    while (pll->sample_nb >= SAMPLE_NB_PER_MS) {
        FLOAT complex buf[SAMPLE_NB_PER_MS];
        int bit;
        double time_stamp;

        time_stamp = pll_build_buffer(pll, buf);
        bit = pll_compute_bit(pll, buf, &time_stamp);
        send_msg_new_pll_bit(pll->sat_nb, bit, time_stamp);
    }

    if (pll->info.consecutive_unlock_event > 128) {
        send_msg_tracking_look_unlock_or_lock_failure(pll->sat_nb);
        return TL_FAIL_STATE;
    }

    return TL_PLL_LOCKED_STATE;
}
