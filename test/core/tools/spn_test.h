#ifndef SPN_TEST_H
#define SPN_TEST_H

// The spn-flavoured half of the harness: everything here needs spn's global
// context to exist. Tests that only exercise a leaf module should include
// "unit.h" directly instead, so their target does not have to link ctx,
// intern and the event buffer.

#include "unit.h"
#include "fixture.h"

#include "ctx/types.h"
#include "event/types.h"

sp_err_t spn_test_ctx_setup(sp_test_t* t);
sp_da(spn_event_t) spn_test_drain_errs(sp_mem_t mem);

#endif
