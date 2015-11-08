#include <stdio.h>

#include "core.h"
#include "file_source.h"
#include "acquisition_basic.h"
#include "tracking_manager.h"
#include "demod_impl.h"
#include "ephemeris_impl.h"

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

    /* configure world */
    acquisition_itf->init(acquisition_itf, 1 << 8/*~0*/);
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

    return 0;
}
