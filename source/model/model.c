#include "model/model.h"

#include "op/types.h"
#include "session/types.h"
#include "unit/unit.h"

spn_err_t resolve(spn_op_t* op);
spn_err_t sync_packages(spn_op_t* op, bool* reresolve);
spn_err_t configure(spn_op_t* op);

spn_err_t spn_model_establish(spn_op_t* op) {
  bool reresolve = sp_zero;
  do {
    spn_try(resolve(op));
    spn_try(sync_packages(op, &reresolve));
  } while (reresolve);
  spn_try(configure(op));
  return spn_units_add_targets(op->session, SPN_UNIT_SCOPE_TARGET);
}
