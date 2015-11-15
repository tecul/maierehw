#ifndef __PVT_4_SAT__
#define __PVT_4_SAT__

#include "pvt.h"
struct pvt_itf *create_pvt_4_sat(void);

struct pvt_info {
    double gps_time;
    double timestamp[4];
    struct eph eph[4];
};

struct position {
    double latitude;
};

#endif
