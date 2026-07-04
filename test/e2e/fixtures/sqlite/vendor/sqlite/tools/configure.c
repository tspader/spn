#include "spn.h"

SPN_EXPORT spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* gen = spn_add_node(config, "codegen");
  spn_node_set_fn(gen, "generate_code");

  spn_node_add_input(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "src/parse.y"));
  spn_node_add_input(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "src/vdbe.c"));
  spn_node_add_input(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "src/sqlite.h.in"));
  spn_node_add_input(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "ext/fts5/fts5parse.y"));
  spn_node_add_input(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "VERSION"));

  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_SOURCE, "sqlite3.c"));
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "sqlite3.h"));

  return SPN_OK;
}
