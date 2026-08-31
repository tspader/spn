#include "unit/unit.h"

#include "hash/digest/digest.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "str/str.h"

typedef struct {
  sp_hash_t qualified;
  sp_hash_t commit;
  sp_hash_t options;
  sp_hash_t deps;
  sp_hash_t patches;
  spn_semver_t version;
  spn_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
  sp_hash_t platform;
  struct {
    sp_hash_t name;
    sp_hash_t cc;
    sp_hash_t cxx;
    sp_hash_t ld;
    sp_hash_t ar;
    sp_hash_t url;
    sp_hash_t identity;
  } toolchain;
} fingerprint_input_t;

// The resolved non-default option set: distinct option sets get distinct
// store paths, while default flips ride the manifest commit hash instead
static sp_hash_t hash_options(spn_session_t* session, spn_pkg_id_t id) {
  spn_resolved_options_t* options = sp_ht_getp(session->options, id);
  if (!options) {
    return 0;
  }

  sp_hash_t hash = 0;
  sp_da_for(*options, it) {
    spn_resolved_option_t* option = &(*options)[it];
    if (option->is_default) {
      continue;
    }
    sp_hash_t parts [] = {
      hash,
      spn_digest_hash_str(option->name),
      (sp_hash_t)option->value.kind,
      option->value.kind == SPN_OPTION_VALUE_STR ? spn_digest_hash_str(option->value.str) : (sp_hash_t)option->value.b,
    };
    hash = spn_digest_hash_combine(parts, sp_carr_len(parts));
  }
  return hash;
}

typedef struct {
  sp_hash_t hash;
  u32 kind;
  u32 private;
} fingerprint_edge_t;

static s32 sort_fingerprint_edges(const void* a, const void* b) {
  const fingerprint_edge_t* lhs = (const fingerprint_edge_t*)a;
  const fingerprint_edge_t* rhs = (const fingerprint_edge_t*)b;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

sp_hash_t spn_unit_fingerprint(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t id) {
  spn_pkg_unit_id_t uid = { .pkg = id, .build = build->id };
  sp_hash_t* memo = sp_ht_getp(session->fingerprints, uid);
  if (memo) {
    return *memo;
  }

  spn_loaded_pkg_t* loaded = sp_ht_getp(session->packages, id);
  spn_pkg_info_t* pkg = loaded->info;

  fingerprint_input_t fingerprint = sp_zero;
  fingerprint.qualified = spn_digest_hash_str(pkg->qualified);
  fingerprint.options = hash_options(session, id);
  fingerprint.version = pkg->version;
  fingerprint.commit = spn_digest_hash_str(pkg->upstream.commit);

  spn_resolved_pkg_t* resolved = sp_ht_getp(session->resolve, id);
  if (resolved) {
    if (resolved->origin.source.kind == SPN_PKG_ROOT_GIT) {
      fingerprint.patches = resolved->origin.source.git.patches.hash;
    }
    if (!sp_da_empty(resolved->edges)) {
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_da(fingerprint_edge_t) edges = sp_da_new(scratch.mem, fingerprint_edge_t);
      sp_da_for(resolved->edges, it) {
        sp_da_push(edges, ((fingerprint_edge_t) {
          .hash = spn_unit_fingerprint(session, build, resolved->edges[it].id),
          .kind = (u32)resolved->edges[it].kind,
          .private = (u32)resolved->edges[it].private,
        }));
      }
      sp_da_sort(edges, sort_fingerprint_edges);
      fingerprint.deps = spn_digest_hash(edges, sp_da_size(edges) * sizeof(fingerprint_edge_t));
      sp_mem_end_scratch(scratch);
    }
  }

  spn_toolchain_info_t* toolchain = build->toolchain->info;
  sp_opt_spn_linkage_t config = spn_session_config_kind(session, pkg->name);

  fingerprint.mode = build->profile.mode;
  fingerprint.opt = build->profile.opt;
  fingerprint.sanitizers = build->profile.sanitizers;
  fingerprint.linkage = config.some ? config.value : build->profile.linkage;
  fingerprint.standard = build->profile.standard;
  fingerprint.arch = build->profile.arch;
  fingerprint.os = build->profile.os;
  fingerprint.abi = build->profile.abi;
  fingerprint.platform = spn_pkg_hash_platform(pkg, &build->profile);
  fingerprint.toolchain.name = spn_digest_hash_str(toolchain->name);
  fingerprint.toolchain.cc = spn_digest_hash_str(toolchain->compiler.program.prefix);
  fingerprint.toolchain.ld = spn_digest_hash_str(toolchain->linker.program.prefix);
  fingerprint.toolchain.ar = spn_digest_hash_str(toolchain->archiver.program.prefix);
  fingerprint.toolchain.cxx = spn_digest_hash_str(toolchain->cxx.program.prefix);
  fingerprint.toolchain.identity = build->toolchain->identity;
  if (!sp_opt_is_null(build->toolchain->artifact)) {
    fingerprint.toolchain.url = spn_digest_hash_str(sp_opt_get(build->toolchain->artifact).sha256);
  }

  sp_hash_t hash = spn_digest_hash(&fingerprint, sizeof(fingerprint));
  sp_ht_insert(session->fingerprints, uid, hash);
  return hash;
}

sp_str_t spn_unit_fingerprint_str(sp_mem_t mem, sp_hash_t fingerprint) {
  return sp_fmt(mem, "{:0>16x}", sp_fmt_uint(fingerprint)).value;
}
