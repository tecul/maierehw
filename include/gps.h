#ifndef __GPS__
#define __GPS__

#include <stddef.h>

#define SAT_NB              32
#define GPS_SAMPLING_RATE   2048000
#define FLOAT               double
#define SAMPLE_NB_PER_MS	(GPS_SAMPLING_RATE / 1000)

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static const double gps_pi = 3.1415926535898;

struct subframe1 {
    int tow;
    int sync_flag;
    int momentum_flag;
    int week_nb;
    int accuracy;
    int health;
    int iodc;
    double tgd;
    int toc;
    double af0;
    double af1;
    double af2;
};

struct subframe2 {
    int tow;
    int sync_flag;
    int momentum_flag;
    int iode;
    double crs;
    double delta_n;
    double m0;
    double cuc;
    double es;
    double cus;
    double square_root_as;
    int toe;
};

struct subframe3 {
    int tow;
    int sync_flag;
    int momentum_flag;
    double cic;
    double omega_e;
    double cis;
    double io;
    double crc;
    double omega;
    double omega_rate;
    int iode;
    double idot;
};

struct eph {
    struct subframe1 sf1;
    struct subframe2 sf2;
    struct subframe3 sf3;
};

#endif
