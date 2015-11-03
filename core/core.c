#include <math.h>

#include "core.h"
#include "ca.h"

void detect_one_satellite(FLOAT complex *one_ms_buffer, int satellite_nb, double treshold_to_use, struct itf_acquisition_freq_range *frange,
                          struct itf_acquisition_shift_range *srange, struct itf_acquisition_sat_status *sat_status)
{
    int j;
    int f;
    int shift;
    double max = 0;
    unsigned int ca_shift = 0;
    int doppler_freq = 0;
    double sum_value = 0;
    int sum_number = 0;
    double treshold;

    for(f = frange->low_freq; f <= frange->high_freq; f+= frange->step) {
        FLOAT complex doppler_samples[2048];
        FLOAT complex samples_mixed[2048];
        double step = 1.0 / 2048000.0;
        double w = 2 * M_PI * f;
        double max_shifted = 0;
        int max_shifted_shift_value = -1;

        /* build doppler samples */
        for(j = 0; j < 2048; j++)
            doppler_samples[j] = 127.0 * cos(w * j * step) + 127.0 * sin(w * j * step) * I;
        /* mix samples so we remove doppler effect */
        for(j = 0; j < 2048; j++)
            samples_mixed[j] = one_ms_buffer[j] * doppler_samples[j];
        /* now mix with ca shifted and compute sum */
        for(shift = srange->low_shift; shift <= srange->high_shift; shift++) {
            FLOAT complex corr = 0;
            for(j = 0; j < 2048; j++)
                corr += samples_mixed[j] * ca[satellite_nb][j + shift];
            if (cabs(corr) > max_shifted) {
                max_shifted = cabs(corr);
                max_shifted_shift_value = shift;
            }
        }
        /* do we have a new max */
        sum_value += max_shifted;
        sum_number++;
        if (max_shifted > max) {
            max = max_shifted;
            ca_shift = max_shifted_shift_value;
            doppler_freq = f;
        }
    }
    /* do we have a sat */
    treshold = treshold_to_use?treshold_to_use * 1.0:1.5 * sum_value / sum_number;
    if (max > treshold) {
        sat_status->is_present = 1;
        sat_status->doppler_freq = doppler_freq;
        sat_status->ca_shift = ca_shift;
        sat_status->treshold_use = treshold;
    } else
        sat_status->is_present = 0;
}
