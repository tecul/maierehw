#ifndef __EPHEMERIS__
#define __EPHEMERIS__

struct ephemeris_itf {
    void (*destroy)(struct ephemeris_itf *ephemeris_itf);
};

#endif
