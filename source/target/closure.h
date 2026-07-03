#ifndef SPN_TARGET_CLOSURE_H
#define SPN_TARGET_CLOSURE_H

#include "sp.h"

#include "forward/types.h"

// Computes the ordered, deduplicated set of packages a link unit (an exe or a
// shared lib) must link against. Walks the transitive package dependency graph
// rooted at the link unit's own package, so a product pulls in everything it
// reaches, not just its direct dependencies.
//
// Recursion stops at shared-library boundaries: a package linked through a
// shared lib is recorded, but its own dependencies were already resolved when
// that shared lib was itself linked, so they are not pulled into the consumer's
// closure. Static, object, and source packages are walked through.
//
// Packages are ordered depender-before-dependee so the linker resolves symbols
// left-to-right. Callers iterate the result's libs/system_deps exactly as they
// did the direct dependency list.
sp_da(spn_pkg_unit_t*) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root);

#endif
