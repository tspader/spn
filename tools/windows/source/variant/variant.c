#include "variant.h"

const winvm_variant_t winvm_variants[] = {
  {
    .name = "base",
    .summary = "golden as-is: no toolchains, exercises the not-installed paths",
    .octet = 201,
  },
  {
    .name = "llvm",
    .summary = "LLVM/clang + lld-link",
    .octet = 202,
    .steps = { { "llvm" } },
  },
  {
    .name = "zig",
    .summary = "zig as a toolchain",
    .octet = 203,
    .steps = { { "zig" } },
  },
  {
    .name = "w64devkit",
    .summary = "w64devkit mingw gcc",
    .octet = 204,
    .steps = { { "w64devkit" } },
  },
  {
    .name = "msys2",
    .summary = "MSYS2 clang64/ucrt64/mingw64 toolchains",
    .octet = 205,
    .steps = { { "msys2", "clang64,ucrt64,mingw64" } },
  },
  {
    .name = "vs2022",
    .summary = "Visual Studio 2022 Build Tools (MSVC)",
    .memory_mb = 8192,
    .vcpus = 6,
    .octet = 206,
    .steps = { { "vs", "2022" } },
  },
  {
    .name = "vs2026",
    .summary = "Visual Studio 2026 Build Tools (MSVC)",
    .memory_mb = 8192,
    .vcpus = 6,
    .octet = 207,
    .steps = { { "vs", "2026" } },
  },
};

const u32 winvm_num_variants = sp_carr_len(winvm_variants);

const winvm_variant_t* winvm_variant_find(const c8* name) {
  sp_carr_for(winvm_variants, it) {
    if (sp_cstr_equal(winvm_variants[it].name, name)) {
      return &winvm_variants[it];
    }
  }
  return SP_NULLPTR;
}

sp_str_t winvm_variant_summary(sp_mem_t mem, const winvm_variant_t* variant) {
  sp_da(sp_str_t) recipes = sp_da_new(mem, sp_str_t);
  sp_carr_for_until(variant->steps, it, variant->steps[it].recipe) {
    sp_da_push(recipes, sp_cstr_as_str(variant->steps[it].recipe));
  }
  if (sp_da_empty(recipes)) {
    return sp_cstr_as_str(variant->summary);
  }
  sp_str_t joined = sp_str_join_n(mem, recipes, (u32)sp_da_size(recipes), sp_str_lit(" + "));
  return sp_fmt(mem, "{} [{}]", sp_fmt_cstr(variant->summary), sp_fmt_str(joined)).value;
}
