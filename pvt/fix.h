#ifndef __FIX__
#define __FIX__

#include "pvt.h"
struct pvt_info {
    double gps_time;
    double timestamp[4];
    struct eph eph[4];
};

struct position {
    double x;
    double y;
    double z;
};

struct coordinate {
	double latitude;
	double longitude;
	double altitude;
};

void compute_fix(struct pvt_info *info, struct position *position);
void compute_coordinate(struct position *position, struct coordinate *coordinate);

#endif
