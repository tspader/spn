#include "resolve.h"

spn_resolve_strategy_t spn_resolve_strategy_from_str(sp_str_t str) {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_RESOLVE_STRATEGY_SOLVER);
}

sp_str_t spn_resolve_strategy_to_str(spn_resolve_strategy_t strategy) {
  switch (strategy) {
    SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
