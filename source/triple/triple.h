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

// Check if `entry` matches `target`. NONE fields in `entry` are wildcards.
bool spn_triple_match(spn_triple_t entry, spn_triple_t target);

// Format a triple for the compiler's --target flag (clang/zig).
// Differs from spn_triple_to_str: mingw -> gnu (zig/clang convention).
sp_str_t spn_triple_to_cc_target(spn_triple_t triple);

// Format a triple for autoconf --host/--build flags.
// Uses GNU convention: x86_64-unknown-linux-gnu, x86_64-w64-mingw32, etc.
sp_str_t spn_triple_to_autoconf(spn_triple_t triple);

// Map target OS enum to CMake's CMAKE_SYSTEM_NAME string.
sp_str_t spn_os_to_cmake_system_name(spn_os_t os);

#endif
