#include "codegen/toolchain.h"

#include "enum/enum.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

static spn_err_t path_issue(spn_path_check_t check) {
  switch (check) {
    case SPN_PATH_UNROOTED: return SPN_ERR_CODEGEN_UNROOTED;
    case SPN_PATH_MALFORMED: return SPN_ERR_CODEGEN_PATH;
    case SPN_PATH_OK: sp_unreachable_case();
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_arg_t lower_program(spn_toml_loader_t* ctx, const c8* key, spn_toolchain_source_t source, spn_path_root_t base, sp_str_t program) {
  spn_arg_t arg = sp_zero;
  program = spn_toml_loader_intern(ctx, sp_fs_normalize_path(ctx->mem, program));
  spn_path_check_t check = spn_toolchain_program(source, base, program, &arg);
  if (check == SPN_PATH_OK) {
    return arg;
  }
  spn_toml_loader_push_key(ctx, key);
  spn_toml_loader_issue_at(ctx, path_issue(check), program);
  spn_toml_loader_pop(ctx);
  return sp_zero_struct(spn_arg_t);
}

static spn_path_t lower_sdk_path(spn_toml_loader_t* ctx, spn_toolchain_source_t source, spn_path_root_t base, sp_str_t sdk) {
  spn_path_t path = sp_zero;
  sdk = spn_toml_loader_intern(ctx, sp_fs_normalize_path(ctx->mem, sdk));
  spn_path_check_t check = spn_toolchain_sdk_path(source, base, sdk, &path);
  if (check == SPN_PATH_OK) {
    return path;
  }
  spn_toml_loader_push_key(ctx, "sdk");
  spn_toml_loader_issue_at(ctx, path_issue(check), sdk);
  spn_toml_loader_pop(ctx);
  return sp_zero_struct(spn_path_t);
}

static void lower_target_caps(spn_toml_loader_t* ctx, spn_toolchain_source_t source, spn_path_root_t base, const spn_cg_toolchain_target_t* cg, spn_toolchain_target_t* target) {
  switch (spn_toolchain_target_caps(cg, target)) {
    case SPN_TARGET_CAPS_OK: break;
    case SPN_TARGET_CAPS_SDK_PATH: target->sdk = lower_sdk_path(ctx, source, base, cg->sdk); break;
    case SPN_TARGET_CAPS_SANITIZERS_FORBIDDEN: spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "sanitizers"); break;
    case SPN_TARGET_CAPS_SDK_FORBIDDEN: spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "sdk"); break;
  }
}

static spn_toolchain_launcher_t lower_launcher(spn_toml_loader_t* ctx, const c8* key, spn_toolchain_source_t source, spn_path_root_t base, sp_str_t str) {
  spn_toolchain_launcher_t launcher = sp_zero;
  if (sp_str_empty(str)) return launcher;

  sp_da(sp_str_t) parts = sp_str_split_c8(ctx->mem, str, ' ');
  launcher.args = sp_da_new(ctx->mem, sp_str_t);
  u32 first = 0;
  while (first < sp_da_size(parts) && sp_str_empty(parts[first])) {
    first++;
  }
  if (first == sp_da_size(parts)) {
    return launcher;
  }
  launcher.program = lower_program(ctx, key, source, base, parts[first]);
  for (u32 it = first + 1; it < sp_da_size(parts); it++) {
    if (!sp_str_empty(parts[it])) {
      sp_da_push(launcher.args, spn_toml_loader_intern(ctx, parts[it]));
    }
  }
  return launcher;
}

static spn_triple_t lower_triple(const spn_cg_toolchain_target_t* target) {
  return (spn_triple_t) {
    .arch = sp_opt_is_null(target->arch) ? SPN_ARCH_NONE : sp_opt_get(target->arch),
    .os   = sp_opt_is_null(target->os)   ? SPN_OS_NONE   : sp_opt_get(target->os),
    .abi  = sp_opt_is_null(target->abi)  ? SPN_ABI_NONE  : sp_opt_get(target->abi),
  };
}

static void issue_triple_entry(spn_toml_loader_t* ctx, spn_triple_entry_t entry) {
  switch (entry) {
    case SPN_TRIPLE_ENTRY_MISSING_ARCH: spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "arch"); break;
    case SPN_TRIPLE_ENTRY_MISSING_OS:   spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "os"); break;
    case SPN_TRIPLE_ENTRY_MISSING_ABI:  spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "abi"); break;
    case SPN_TRIPLE_ENTRY_FOREIGN_ARCH: spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "arch"); break;
    case SPN_TRIPLE_ENTRY_FOREIGN_ABI:  spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "abi"); break;
    case SPN_TRIPLE_ENTRY_OK: sp_unreachable_case();
  }
}

static bool lower_linker(spn_toml_loader_t* ctx, spn_ld_family_t declared, spn_cc_driver_t driver) {
  if (!spn_ld_accepts(driver, declared)) {
    spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "linker");
    return false;
  }
  return declared == SPN_LD_FAMILY_LLD;
}

static bool target_has_fields(const spn_cg_toolchain_target_t* cg) {
  return !sp_opt_is_null(cg->arch) || !sp_opt_is_null(cg->os) || !sp_opt_is_null(cg->abi) || !sp_str_empty(cg->sdk) || !sp_da_empty(cg->sanitizers);
}

// "host" names the rows the driver derives on the machine spn runs on. It
// carries nothing else, and it appears at most once.
static bool lower_host_row(spn_toml_loader_t* ctx, const spn_cg_toolchain_target_t* cg, bool* host_row) {
  if (!sp_str_equal_cstr(cg->kind, "host") || target_has_fields(cg) || *host_row) {
    spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "kind");
    return false;
  }
  *host_row = true;
  return true;
}

static sp_da(spn_toolchain_target_t) lower_toolchain_targets(spn_toml_loader_t* ctx, spn_cc_driver_t driver, spn_toolchain_source_t source, spn_path_root_t base, sp_da(spn_cg_toolchain_target_t) cg, bool* host_row) {
  sp_da(spn_toolchain_target_t) targets = sp_da_new(ctx->mem, spn_toolchain_target_t);
  *host_row = sp_da_empty(cg);
  spn_toml_loader_push_key(ctx, "target");
  sp_da_for(cg, it) {
    if (!sp_str_empty(cg[it].kind)) {
      spn_toml_loader_push_index(ctx, it);
      lower_host_row(ctx, &cg[it], host_row);
      spn_toml_loader_pop(ctx);
      continue;
    }
    spn_triple_t partial = lower_triple(&cg[it]);
    spn_toolchain_target_t target = sp_zero;
    spn_triple_entry_t entry = spn_triple_entry(partial, &target.triple);
    if (entry != SPN_TRIPLE_ENTRY_OK) {
      spn_toml_loader_push_index(ctx, it);
      issue_triple_entry(ctx, entry);
      spn_toml_loader_pop(ctx);
      continue;
    }
    if (!spn_toolchain_driver_composes(driver, spn_ld_dialect(target.triple))) {
      spn_toml_loader_push_index(ctx, it);
      spn_toml_loader_issue_at(ctx, SPN_ERR_CODEGEN_INVALID, spn_triple_to_str(ctx->mem, target.triple));
      spn_toml_loader_pop(ctx);
      continue;
    }
    spn_toml_loader_push_index(ctx, it);
    lower_target_caps(ctx, source, base, &cg[it], &target);
    spn_toml_loader_pop(ctx);
    sp_da_push(targets, target);
  }
  spn_toml_loader_pop(ctx);
  return targets;
}

static sp_da(sp_str_t) lower_strs(spn_toml_loader_t* ctx, sp_da(sp_str_t) values) {
  sp_da(sp_str_t) out = sp_da_new(ctx->mem, sp_str_t);
  sp_da_for(values, it) {
    sp_da_push(out, spn_toml_loader_intern(ctx, values[it]));
  }
  return out;
}

spn_toolchain_decl_t spn_toolchain_lower(spn_toml_loader_t* ctx, u32 at, spn_path_root_t base, const spn_cg_toolchain_decl_t* decl) {
  spn_toml_loader_push_key(ctx, "toolchain");
  spn_toml_loader_push_index(ctx, at);

  if (sp_str_empty(decl->name))     { spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "name"); }
  if (spn_toolchain_ref_from_str(decl->name).kind == SPN_TOOLCHAIN_REF_AUTO) { spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "name"); }
  if (sp_str_empty(decl->compiler)) { spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "compiler"); }
  if (sp_str_empty(decl->archiver)) { spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "archiver"); }
  if (sp_opt_is_null(decl->driver) || sp_opt_get(decl->driver) == SPN_CC_DRIVER_NONE) {
    spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "driver");
  }

  spn_toolchain_decl_t toolchain = sp_zero;
  toolchain.name = decl->name;
  toolchain.version = decl->version;
  toolchain.driver = sp_opt_is_null(decl->driver) ? SPN_CC_DRIVER_NONE : sp_opt_get(decl->driver);
  toolchain.link_args = lower_strs(ctx, decl->link_args);

  toolchain.hosts = sp_da_new(ctx->mem, spn_toolchain_host_t);
  sp_da_for(decl->host, it) {
    bool url = !sp_str_empty(decl->host[it].value.url);
    bool sha = !sp_str_empty(decl->host[it].value.sha256);
    if (url && !sha) {
      spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "sha256");
    }
    if (sha && !url) {
      spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "url");
    }

    spn_triple_t host = sp_zero;
    if (spn_triple_parse_host(decl->host[it].key, &host)) {
      spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_INVALID, "host");
      continue;
    }
    sp_da_push(toolchain.hosts, ((spn_toolchain_host_t) {
      .triple = host,
      .artifact = {
        .url = decl->host[it].value.url,
        .sha256 = decl->host[it].value.sha256,
        .mirror_list = decl->mirrors,
      },
    }));
  }
  toolchain.source = spn_toolchain_source(toolchain.hosts);
  if (toolchain.source == SPN_TOOLCHAIN_SOURCE_MIXED) {
    spn_toml_loader_push_key(ctx, "host");
    sp_da_for(toolchain.hosts, it) {
      if (sp_str_empty(toolchain.hosts[it].artifact.url)) {
        spn_toml_loader_push_key(ctx, sp_str_to_cstr(ctx->mem, spn_triple_to_str(ctx->mem, toolchain.hosts[it].triple)));
        spn_toml_loader_issue(ctx, SPN_ERR_CODEGEN_MISSING_KEY, "url");
        spn_toml_loader_pop(ctx);
      }
    }
    spn_toml_loader_pop(ctx);
  }

  toolchain.compiler = lower_launcher(ctx, "compiler", toolchain.source, base, decl->compiler);
  toolchain.cxx = lower_launcher(ctx, "cxx", toolchain.source, base, decl->cxx);
  toolchain.archiver = lower_launcher(ctx, "archiver", toolchain.source, base, decl->archiver);
  if (toolchain.driver) {
    toolchain.lld = lower_linker(ctx, sp_opt_is_null(decl->linker) ? SPN_LD_FAMILY_NONE : sp_opt_get(decl->linker), toolchain.driver);
    toolchain.targets = lower_toolchain_targets(ctx, toolchain.driver, toolchain.source, base, decl->target, &toolchain.host_row);
  }

  spn_toml_loader_pop(ctx);
  spn_toml_loader_pop(ctx);
  return toolchain;
}

void spn_toolchains_parse(spn_toml_loader_t* ctx, sp_str_t toml, spn_cg_config_t* out) {
  toml_table_t* table = spn_codegen_parse_str(ctx, toml);
  if (table) {
    spn_config_read(ctx, table, out);
    toml_free(table);
  }
}
