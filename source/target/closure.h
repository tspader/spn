#ifndef SPN_TARGET_CLOSURE_H
#define SPN_TARGET_CLOSURE_H

#include "sp.h"

#include "forward/types.h"

typedef struct {
  spn_pkg_unit_t* pkg;
  bool private;
} spn_closure_entry_t;

// Computes the ordered, deduplicated set of packages a link unit (an exe or a
// shared lib) must link against. Walks the transitive package dependency graph
// rooted at the link unit's own package, so a product pulls in everything it
// reaches, not just its direct dependencies. Only link edges are walked: build
// deps (and test deps, unless the link unit is a test) live in other units and
// never land on the link line.
//
// Recursion stops at shared-library boundaries: a package linked through a
// shared lib is recorded, but its own dependencies were already resolved when
// that shared lib was itself linked, so they are not pulled into the consumer's
// closure. Static, object, and source packages are walked through.
//
// An entry is private if any edge on its path from the root is private; a
// shared lib hides those symbols so its embedded copy can't collide with a
// consumer's own instance.
//
// Packages are ordered depender-before-dependee so the linker resolves symbols
// left-to-right. Callers iterate the result's libs/system_deps exactly as they
// did the direct dependency list.
sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root);

#endif
