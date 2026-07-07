#ifndef SPN_FUZZ_RESOLVER_H
#define SPN_FUZZ_RESOLVER_H

#include "sp.h"

#include "index/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "target/types.h"
#include "sp_fuzz.h"

#define FZ_MAX_PKGS 128
#define FZ_SMALL_PKGS 6
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
  FZ_ERR_BUDGET_EXHAUSTED,
  FZ_ERR_PLANTED_REJECTED,
  FZ_ERR_INCOMPLETE,
  FZ_ERR_ROOT_MISSING,
  FZ_ERR_EDGE_MISSING,
  FZ_ERR_EDGE_EXTRA,
  FZ_ERR_EDGE_MISCLASSIFIED,
  FZ_ERR_EDGE_OUT_OF_RANGE,
  FZ_ERR_UNIT_DUPLICATE,
  FZ_ERR_GRAPH_CYCLE,
  FZ_ERR_SHARED_DUP_MISSED,
  FZ_ERR_IDENTITY_SPLIT,
  FZ_ERR_SHUFFLE_VERDICT,
  FZ_ERR_SHUFFLE_SOLUTION,
  FZ_ERR_RENAME_VERDICT,
  FZ_ERR_INTERN_VERDICT,
  FZ_ERR_INTERN_SOLUTION,
  FZ_ERR_PIN_VERDICT,
  FZ_ERR_PIN_SOLUTION,
  FZ_ERR_EXTEND_VERDICT,
  FZ_ERR_COUNT,
} fz_err_t;

typedef enum {
  FZ_RANGE_EXACT,
  FZ_RANGE_BARE,
  FZ_RANGE_CARET,
  FZ_RANGE_TILDE,
  FZ_RANGE_TILDE_MAJOR,
  FZ_RANGE_GT,
  FZ_RANGE_GEQ,
  FZ_RANGE_LEQ,
  FZ_RANGE_LT,
  FZ_RANGE_STAR_MINOR,
  FZ_RANGE_STAR_PATCH,
  FZ_RANGE_ANY,
  FZ_RANGE_COUNT,
} fz_range_shape_t;

typedef struct {
  u32 pkg;
  fz_range_shape_t shape;
  spn_semver_t version;
  spn_index_dep_kind_t kind;
  bool private;
} fz_dep_t;

typedef struct {
  spn_semver_t version;
  sp_da(fz_dep_t) deps;
} fz_release_t;

typedef struct {
  bool local;
  bool has_config;
  spn_linkage_t config;
  spn_linkage_set_t linkages;
  sp_da(fz_release_t) releases;
} fz_pkg_t;

typedef struct {
  u64 shapes[FZ_RANGE_COUNT];
  u64 pkg_count;
  u64 release_count;
  u64 density;
  u64 out_degree;
  u64 back_density;
  u64 build_pct;
  u64 test_pct;
  u64 private_pct;
  u64 shared_pct;
  u64 static_pct;
  u64 config_pct;
  u64 local_pct;
  spn_linkage_t linkage;
  bool big;
  bool planted;
  bool features;
  u64 budget;
} fz_profile_t;

typedef struct {
  fz_profile_t profile;
  sp_da(fz_pkg_t) pkgs;
  sp_da(fz_dep_t) roots;
  sp_da(u32) plan;
  bool planted;
} fz_universe_t;

typedef struct {
  s32 pkg;
  spn_semver_t version;
  sp_hash_t hash;
} fz_pick_t;

typedef sp_da(fz_pick_t) fz_solution_t;

extern const c8* fz_root_qualified;

#define try(expr) do { fz_err_t __err = (expr); if (__err) return __err; } while (0)
#define must(expr, err) do { if (!(expr)) return err; } while (0)

sp_str_t fz_err_to_str(fz_err_t err);

sp_str_t      fz_pkg_name(u32 pkg);
bool          fz_pkg_shared(fz_universe_t* u, s32 pkg);
bool          fz_ranges_agree(void);
bool          fz_range_sat(fz_dep_t dep, spn_semver_t version);
sp_str_t      fz_range_render(sp_mem_t mem, fz_dep_t dep);
s32           fz_pkg_from_qualified(fz_universe_t* u, sp_str_t qualified);
fz_profile_t  fz_gen_profile(sp_fuzz_prng_t* prng);
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile);
fz_universe_t fz_shuffle_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u);
fz_universe_t fz_rename_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u);
fz_universe_t fz_pin_universe(sp_mem_t mem, fz_universe_t* u, fz_solution_t solution);
fz_universe_t fz_extend_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u);

bool fz_oracle_sat(fz_universe_t* u);
bool fz_plan_sat(fz_universe_t* u);

fz_err_t      fz_check_solve(fz_universe_t* u, spn_resolve_query_t* query);
fz_err_t      fz_check_units(sp_mem_t mem, fz_universe_t* u, spn_resolve_query_t* query);
fz_solution_t fz_solution(sp_mem_t mem, fz_universe_t* u, spn_resolve_query_t* query);
bool          fz_solution_equal(fz_solution_t a, fz_solution_t b);

void fz_dump(fz_universe_t* u, u64 iter);

#endif
