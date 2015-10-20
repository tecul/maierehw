#include <stdio.h>

#include "core.h"
#include "file_source.h"
#include "acquisition_basic.h"

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

    /* configure world */
    acquisition_itf->init(acquisition_itf, ~0);
    /* run it */
    while(source_itf->read_one_ms(source_itf, msg_payload_new_one_ms_buffer.file_source_buffer) == 0) {
        publish(&msg);
        msg_payload_new_one_ms_buffer.first_sample_time_in_ms++;
    }

    /* destroy world */
    source_itf->destroy(source_itf);
    acquisition_itf->destroy(acquisition_itf);

    return 0;
}
