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

#endif
