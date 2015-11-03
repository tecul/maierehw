#ifndef __TRACKING__
#define __TRACKING__

struct tracking_manager_itf {
    void (*destroy)(struct tracking_manager_itf *mgr);
};

struct tracking_loop_itf {
	void (*destroy)(struct tracking_loop_itf *tracking_loop_itf);
};

#endif
