#include "spn.h"

s32 gen_orphan(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "generating orphan_header.h (no consumers)...");
  spn_write_file(spn, "orphan_header.h",
    "#ifndef ORPHAN_H\n"
    "#define ORPHAN_H\n"
    "#define ORPHAN_VALUE 42\n"
    "#endif\n"
  );
  return 0;
}

s32 gen_consumed(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "generating consumed_header.h...");
  spn_write_file(spn, "consumed_header.h",
    "#ifndef CONSUMED_H\n"
    "#define CONSUMED_H\n"
    "#define CONSUMED_VALUE 100\n"
    "#endif\n"
  );
  return 0;
}

s32 consumer_fn(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "consumer: reading consumed_header.h...");
  return 0;
}

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  const c8* orphan_h = spn_get_subdir(spn, SPN_DIR_WORK, "orphan_header.h");
  const c8* consumed_h = spn_get_subdir(spn, SPN_DIR_WORK, "consumed_header.h");

  spn_node_t* orphan = spn_add_node(config, "gen_orphan");
  spn_node_set_fn(orphan, gen_orphan);
  spn_node_add_output(orphan, orphan_h);

  spn_node_t* producer = spn_add_node(config, "gen_consumed");
  spn_node_set_fn(producer, gen_consumed);
  spn_node_add_output(producer, consumed_h);

  spn_node_t* consumer = spn_add_node(config, "consumer");
  spn_node_set_fn(consumer, consumer_fn);
  spn_node_add_input(consumer, consumed_h);
  return SPN_OK;
}
