#ifndef __EPHEMERIS__
#define __EPHEMERIS__

struct ephemeris_itf {
    void (*destroy)(struct ephemeris_itf *ephemeris_itf);
};

struct ephemeris_dumper_itf {
    void (*destroy)(struct ephemeris_dumper_itf *ephemeris_dumper_itf);
};

#endif
