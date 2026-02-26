#include "spn.h"

s32 gen_orphan(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "generating orphan_header.h (no consumers)...");
  spn_write_file(spn_node_ctx_get_build(ctx), "orphan_header.h",
    "#ifndef ORPHAN_H\n"
    "#define ORPHAN_H\n"
    "#define ORPHAN_VALUE 42\n"
    "#endif\n"
  );
  return 0;
}

s32 gen_consumed(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "generating consumed_header.h...");
  spn_write_file(spn_node_ctx_get_build(ctx), "consumed_header.h",
    "#ifndef CONSUMED_H\n"
    "#define CONSUMED_H\n"
    "#define CONSUMED_VALUE 100\n"
    "#endif\n"
  );
  return 0;
}

s32 consumer_fn(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "consumer: reading consumed_header.h...");
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  const c8* orphan_h = spn_get_subdir(ctx, SPN_DIR_WORK, "orphan_header.h");
  const c8* consumed_h = spn_get_subdir(ctx, SPN_DIR_WORK, "consumed_header.h");

  spn_node_t* orphan = spn_add_node(ctx, "gen_orphan");
  spn_node_set_fn(orphan, gen_orphan);
  spn_node_add_output(orphan, orphan_h);

  spn_node_t* producer = spn_add_node(ctx, "gen_consumed");
  spn_node_set_fn(producer, gen_consumed);
  spn_node_add_output(producer, consumed_h);

  spn_node_t* consumer = spn_add_node(ctx, "consumer");
  spn_node_set_fn(consumer, consumer_fn);
  spn_node_add_input(consumer, consumed_h);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
