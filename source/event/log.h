#ifndef SPN_EVENT_LOG_H
#define SPN_EVENT_LOG_H

#include "event/types.h"

void spn_event_log_init(sp_mem_t mem);
void spn_event_log_jsonl(sp_io_writer_t* out, spn_build_event_t* event);
void spn_event_log_build(sp_io_writer_t* out, spn_build_event_t* event);
void spn_json_write_str(sp_io_writer_t* out, sp_str_t str);

#endif
