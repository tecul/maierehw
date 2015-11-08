#ifndef __DEMOD__
#define __DEMOD__

struct demod_itf {
    void (*destroy)(struct demod_itf *demod_itf);
};

struct demod_word_itf {
    void (*destroy)(struct demod_word_itf *demod_word_itf);
};

#endif
