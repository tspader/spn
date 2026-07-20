#include "spn.h"

#include <stdio.h>

SPN_EXPORT
s32 gen(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen: writing gen.h");
  spn_io_write("/work/gen.h",
    "#ifndef GEN_H\n"
    "#define GEN_H\n"
    "#define GEN 1\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* node = spn_add_node(config, "gen");
  spn_node_set_fn(node, "gen");
  spn_node_add_output(node, spn_get_subdir(spn, SPN_DIR_WORK, "gen.h"));

  FILE* order = fopen("/source/inputs.txt", "r");
  if (!order) {
    return SPN_ERROR;
  }

  char line [128];
  while (fgets(line, sizeof(line), order)) {
    u32 len = 0;
    while (line[len] && line[len] != '\n') {
      len++;
    }
    line[len] = 0;
    if (len == 0) {
      continue;
    }
    spn_node_add_input(node, spn_get_subdir(spn, SPN_DIR_SOURCE, line));
  }

  fclose(order);
  return SPN_OK;
}
