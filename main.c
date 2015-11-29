#include <stdio.h>

#include "core.h"
#include "file_source.h"
#include "acquisition_basic.h"
#include "tracking_manager.h"
#include "demod.h"

#if 1
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

    /* start */
    publish(allocate_event(EVT_QUEUE_EMPTY));
    event_loop();

    /* destroy world */
    destroy_file_source(file_source_handle);
    destroy_acquisition_basic(acquisition_basic);
    destroy_tracking_manager(tracking_manager);
    destroy_demod(demod);
    destroy_demod_word(demod_word);
}
#else
int main(int argc, char **argv)
{
    struct msg_payload_new_one_ms_buffer msg_payload_new_one_ms_buffer;
    struct msg msg;

    msg.msg_type = "new_one_ms_buffer";
    msg.msg_payload = &msg_payload_new_one_ms_buffer;
    msg_payload_new_one_ms_buffer.first_sample_time_in_ms = 0;
    /* create world */
    struct source_itf *source_itf = create_file_source(argv[1]);
    struct acquisition_itf *acquisition_itf = create_acquisition_basic();
    struct tracking_manager_itf *tracking_manager_itf = create_tracking_manager(8);
    struct demod_itf *demod_itf = create_demod();
    struct demod_word_itf *demod_word_itf = create_demod_word();
    struct ephemeris_itf *ephemeris_itf = create_ephemeris();
    //struct ephemeris_dumper_itf *ephemeris_dumper_itf = create_ephemeris_dumper();
    //struct ephemeris_loader_itf *ephemeris_loader_itf = create_ephemeris_loader();
    struct pvt_itf *pvt_itf = create_pvt_4_sat();
    struct pvt_cook_itf *pvt_cook_itf = create_pvt_cook();

    /* configure world */
    acquisition_itf->init(acquisition_itf, /*(1 << 29) | (1 << 7) | (1 << 8) | (1 << 27)*/   /*1 << 8*/~0);
    /* run it */
    while(source_itf->read_one_ms(source_itf, msg_payload_new_one_ms_buffer.file_source_buffer) == 0) {
        publish(&msg);
        msg_payload_new_one_ms_buffer.first_sample_time_in_ms++;
    }

    /* destroy world */
    source_itf->destroy(source_itf);
    acquisition_itf->destroy(acquisition_itf);
    tracking_manager_itf->destroy(tracking_manager_itf);
    demod_itf->destroy(demod_itf);
    demod_word_itf->destroy(demod_word_itf);
    ephemeris_itf->destroy(ephemeris_itf);
    //ephemeris_dumper_itf->destroy(ephemeris_dumper_itf);
    //ephemeris_loader_itf->destroy(ephemeris_loader_itf);
    pvt_itf->destroy(pvt_itf);
    pvt_cook_itf->destroy(pvt_cook_itf);

    return 0;
}
#endif
