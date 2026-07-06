#ifndef SPN_FUZZ_RESOLVER_H
#define SPN_FUZZ_RESOLVER_H

#include "sp.h"

#include "resolve/types.h"
#include "semver/types.h"
#include "sp_fuzz.h"

#define FZ_MAX_PKGS 6
#define FZ_MAX_RELEASES 4

typedef enum {
  FZ_OK = 0,
  FZ_ERR_PLANTED_UNSAT,
  FZ_ERR_SOLVE_FOREIGN_PKG,
  FZ_ERR_SOLVE_DUPLICATE_INSTANCE,
  FZ_ERR_SOLVE_FOREIGN_VERSION,
  FZ_ERR_ROOT_UNRESOLVED,
  FZ_ERR_ROOT_OUT_OF_RANGE,
  FZ_ERR_DEP_UNRESOLVED,
  FZ_ERR_DEP_OUT_OF_RANGE,
  FZ_ERR_EVENT_MISSING,
  FZ_ERR_PLANTED_REJECTED,
  FZ_ERR_INCOMPLETE,
} fz_err_t;

typedef enum {
  FZ_RANGE_EXACT,
  FZ_RANGE_CARET,
  FZ_RANGE_TILDE,
  FZ_RANGE_GEQ,
  FZ_RANGE_LEQ,
  FZ_RANGE_LT,
  FZ_RANGE_ANY,
  FZ_RANGE_COUNT,
} fz_range_shape_t;

typedef struct {
  u32 pkg;
  fz_range_shape_t shape;
  spn_semver_t version;
} fz_dep_t;

typedef struct {
  spn_semver_t version;
  sp_da(fz_dep_t) deps;
} fz_release_t;

typedef struct {
  sp_da(fz_release_t) releases;
} fz_pkg_t;

typedef struct {
  sp_da(fz_pkg_t) pkgs;
  sp_da(fz_dep_t) roots;
  bool planted;
} fz_universe_t;

extern const c8* fz_names[FZ_MAX_PKGS];
extern const c8* fz_root_qualified;

sp_str_t fz_err_to_str(fz_err_t err);

bool          fz_range_sat(fz_dep_t dep, spn_semver_t version);
sp_str_t      fz_range_render(sp_mem_t mem, fz_dep_t dep);
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng);

bool fz_oracle_sat(fz_universe_t* u);

fz_err_t fz_check_solve(fz_universe_t* u, spn_resolve_query_t* query);

void fz_dump(fz_universe_t* u, u64 iter);

#endif
