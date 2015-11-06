#ifndef __DEMOD__
#define __DEMOD__

struct demod_itf {
    void (*destroy)(struct demod_itf *demod_itf);
};

#endif
