#ifndef __ACQUISITION__
#define __ACQUISITION__

#include <stdint.h>
#include <complex.h>
#include "gps.h"

/* receive 1ms of data and return satellite status. data FLOAT complex sample at
   GPS_SAMPLING_RATE Mhz.
*/

struct acquisition_itf {
    int (*init)(struct acquisition_itf *acq, uint32_t satellites);
    void (*destroy)(struct acquisition_itf *acq);
};

#endif
