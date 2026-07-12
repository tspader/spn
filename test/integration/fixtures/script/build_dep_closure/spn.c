#include "spn.h"

#include "alpha.h"

#if ALPHA_VALUE != 7
#error "alpha.h from the build dep was not on the include path"
#endif

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));
  spn_io_write("/work/closure.h",
    "#ifndef CLOSURE_H\n"
    "#define CLOSURE_H\n"
    "#define CLOSURE_VALUE 7\n"
    "#endif\n"
  );
  return SPN_OK;
}
