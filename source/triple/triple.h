#ifndef SPN_TRIPLE_TRIPLE_H
#define SPN_TRIPLE_TRIPLE_H

#include "sp.h"
#include "spn.h"

// Parse a triple string like "aarch64-linux-gnu" into a spn_triple_t.
// Components can be omitted: "aarch64-linux" or "aarch64" are valid.
// Returns a triple with NONE for any missing components.
spn_triple_t spn_triple_from_str(sp_str_t str);

// Format a triple as "arch-os-abi". Omits trailing NONE components.
sp_str_t spn_triple_to_str(spn_triple_t triple);

// Detect the host platform triple.
spn_triple_t spn_triple_host(void);

// Fill NONE fields in `partial` with values from `base`.
spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial);

#endif
