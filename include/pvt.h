#ifndef __PVT__
#define __PVT__

struct pvt_itf {
    void (*destroy)(struct pvt_itf *pvt_itf);
};

#endif
