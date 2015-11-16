#ifndef __PVT__
#define __PVT__

struct pvt_itf {
    void (*destroy)(struct pvt_itf *pvt_itf);
};

struct pvt_cook_itf {
    void (*destroy)(struct pvt_cook_itf *pvt_cook_itf);
};

#endif
