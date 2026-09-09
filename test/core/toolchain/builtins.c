#include "toolchain.h"

static bool builtins_is_sha256(sp_str_t str) {
  return str.len == 64 && test_str_is_hex(str);
}

static sp_err_t builtins_decls(sp_test_t* t, sp_da(spn_toolchain_decl_t)* decls) {
  sp_str_t json = sp_zero;
  if (spn_test_builtin_json(t, &json)) {
    return SP_ERR;
  }
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_decls_parse(sp_test_arena(t), json, decls));
  return SP_OK;
}

sp_test(builtins, declared_order) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (spn_test_builtin_catalog(t, &catalog, (spn_triple_t) HOST_X64_LINUX)) {
    return SP_ERR;
  }

  const c8* order [] = { "zig", "msvc", "clang", "llvm", "gcc" };
  sp_must_eq(t, (u32)sp_carr_len(order), fixture_catalog_size(&catalog));
  sp_carr_for(order, it) {
    sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, it)->name, order[it]);
  }
  return SP_OK;
}

sp_test(builtins, well_formed) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  if (builtins_decls(t, &decls)) {
    return SP_ERR;
  }
  sp_must(t, !sp_da_empty(decls));

  sp_da_for(decls, it) {
    spn_toolchain_decl_t* decl = &decls[it];
    sp_expect(t, !spn_arg_empty(decl->cxx.program));

    switch (decl->source) {
      case SPN_TOOLCHAIN_SOURCE_LOCAL: {
        break;
      }
      case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
        sp_expect(t, !sp_str_empty(decl->version));
        sp_da_for(decl->hosts, ht) {
          spn_artifact_t artifact = decl->hosts[ht].artifact;
          sp_expect(t, sp_str_starts_with(artifact.url, sp_str_lit("https://")));
          sp_expect(t, builtins_is_sha256(artifact.sha256));
          sp_expect(t, !sp_str_empty(artifact.mirror_list));
        }
        break;
      }
      case SPN_TOOLCHAIN_SOURCE_MIXED: {
        sp_unreachable_case();
      }
    }
  }

  return SP_OK;
}
