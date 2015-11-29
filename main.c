#include <stdio.h>

#include "core.h"
#include "file_source.h"
#include "acquisition_basic.h"
#include "tracking_manager.h"
#include "demod.h"
#include "ephemeris.h"
#include "pvt.h"

int main(int argc , char **argv)
{
    /* first need to init event_module */
    init_event_module();

    /* create world */
    handle file_source_handle = create_file_source(argv[1]);
    handle acquisition_basic = create_acquisition_basic((1 << 29) | (1 << 7) | (1 << 8) | (1 << 27)/*1 << 8*//*~0*/);
    handle tracking_manager = create_tracking_manager(8);
    handle demod = create_demod();
    handle demod_word = create_demod_word();
    handle eph = create_ephemeris();
    handle pvt = create_pvt_4_sat();
    handle pvt_cook = create_pvt_cook();

    /* start */
    publish(allocate_event(EVT_QUEUE_EMPTY));
    event_loop();

    /* destroy world */
    destroy_file_source(file_source_handle);
    destroy_acquisition_basic(acquisition_basic);
    destroy_tracking_manager(tracking_manager);
    destroy_demod(demod);
    destroy_demod_word(demod_word);
    destroy_ephemeris(eph);
    destroy_pvt_4_sat(pvt);
    destroy_pvt_cook(pvt_cook);
}
