#ifndef __SOURCE__
#define __SOURCE__

#include <complex.h>
#include "gps.h"

struct source_itf {
	int (*read_one_ms)(struct source_itf *, FLOAT complex *);
	void (*destroy)(struct source_itf *);
};

#endif
