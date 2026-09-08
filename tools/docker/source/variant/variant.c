#include "variant.h"

typedef struct {
  const c8* template;
  const c8* packages;
  const c8* label;
} distro_info_t;

static const distro_info_t distros [] = {
  [DISTRO_DEBIAN] = {
    .template = "debian",
    .packages = "ca-certificates git curl xz-utils",
    .label = "glibc",
  },
  [DISTRO_UBUNTU20] = {
    .template = "ubuntu20",
    .packages = "ca-certificates git curl xz-utils",
    .label = "glibc 2.31",
  },
  [DISTRO_UBUNTU19] = {
    .template = "ubuntu19",
    .packages = "ca-certificates git curl xz-utils",
    .label = "glibc 2.29",
  },
  [DISTRO_CENTOS7] = {
    .template = "centos7",
    .packages = "ca-certificates git curl xz",
    .label = "glibc 2.17",
  },
  [DISTRO_ALPINE] = {
    .template = "alpine",
    .packages = "ca-certificates git curl xz",
    .label = "musl, busybox /bin/sh",
  },
};

const sysroot_t sysroots [] = {
  [SYSROOT_MUSL] = {
    .name = "musl",
    .path = "/sysroot/musl",
    .packages = { "musl", "musl-dev", "musl-tools" },
    .kind = INSTALL_LINKS,
    .links = {
      { "include", "/usr/include/x86_64-linux-musl" },
      { "lib", "/usr/lib/x86_64-linux-musl" },
    },
  },
  [SYSROOT_WASI] = {
    .name = "wasi",
    .path = "/usr",
    .packages = { "wasi-libc", "libclang-rt-dev-wasm32" },
    .kind = INSTALL_PACKAGES,
  },
  [SYSROOT_ARM64] = {
    .name = "arm64",
    .path = "/sysroot/arm64",
    .kind = INSTALL_DEBS,
    .debs = {
      .arch = "arm64",
      .names = { "libc6", "libc6-dev", "linux-libc-dev", "libgcc-s1", "libgcc-12-dev" },
    },
  },
  [SYSROOT_WASI_SDK] = {
    .name = "wasi-sdk",
    .path = "/opt/wasi-sdk",
    .kind = INSTALL_ARTIFACT,
    .artifact = LANE_WASI_SDK,
  },
};

static const c8* lane_names [LANE_COUNT] = {
  [LANE_GCC]             = "gcc",
  [LANE_CLANG]           = "clang",
  [LANE_LLVM]            = "llvm",
  [LANE_ZIG]             = "zig",
  [LANE_GCC_LLD]         = "gcc-lld",
  [LANE_AARCH64_GNU]     = "aarch64-gnu",
  [LANE_MINGW_GNU]       = "mingw-gnu",
  [LANE_CLANG_MINGW]     = "clang-mingw",
  [LANE_CLANG_MINGW_LLD] = "clang-mingw-lld",
  [LANE_CLANG_BARE]      = "clang-bare",
  [LANE_MUSL_GCC]        = "musl-gcc",
  [LANE_CLANG_MUSL]      = "clang-musl",
  [LANE_CLANG_WASI]      = "clang-wasi",
  [LANE_WASI_SDK]        = "wasi-sdk",
  [LANE_WASI_SDK_LOCAL]  = "wasi-sdk-local",
  [LANE_CLANG_SYSROOT]   = "clang-sysroot",
  [LANE_CLANG_CROSS]     = "clang-cross",
  [LANE_CLANG_MSVC]      = "clang-msvc",
};

static const compiler_t compilers [] = { COMPILER_GCC, COMPILER_GXX, COMPILER_CLANG };

const variant_t variants [] = {
  {
    .name = "debian-gcc",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .extra = { "lld" },
    .lanes = { LANE_GCC, LANE_GCC_LLD },
  },
  {
    .name = "debian-clang",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .lanes = { LANE_CLANG },
  },
  {
    .name = "debian-llvm",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_GCC | COMPILER_CLANG,
    .check = LANE_CLANG,
    .extra = { "lld", "llvm" },
    .lanes = { LANE_LLVM, LANE_CLANG, LANE_GCC, LANE_GCC_LLD, LANE_CLANG_BARE },
  },
  {
    .name = "debian-cross",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .extra = { "gcc-aarch64-linux-gnu", "libc6-dev-arm64-cross" },
    .lanes = { LANE_AARCH64_GNU, LANE_CLANG_CROSS },
  },
  {
    .name = "debian-mingw",
    .distro = DISTRO_DEBIAN,
    .extra = { "gcc-mingw-w64-x86-64" },
    .lanes = { LANE_MINGW_GNU },
  },
  {
    .name = "debian-mingw-clang",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .extra = { "gcc-mingw-w64-x86-64", "lld" },
    .lanes = { LANE_CLANG_MINGW, LANE_CLANG_MINGW_LLD },
  },
  {
    .name = "debian-musl",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_GCC | COMPILER_CLANG,
    .check = LANE_GCC,
    .sysroots = { SYSROOT_MUSL },
    .lanes = { LANE_MUSL_GCC, LANE_CLANG_MUSL },
  },
  {
    .name = "debian-wasi",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .extra = { "lld", "llvm" },
    .sysroots = { SYSROOT_WASI },
    .lanes = { LANE_CLANG_WASI },
  },
  {
    .name = "debian-wasi-sdk",
    .distro = DISTRO_DEBIAN,
    .sysroots = { SYSROOT_WASI_SDK },
    .lanes = { LANE_WASI_SDK, LANE_WASI_SDK_LOCAL },
  },
  {
    .name = "debian-sysroot",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .extra = { "lld", "llvm" },
    .sysroots = { SYSROOT_ARM64 },
    .lanes = { LANE_CLANG_SYSROOT },
  },
  {
    .name = "debian-cc-only",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_GCC,
    .check = LANE_GCC,
    .lanes = { LANE_GCC },
  },
  {
    .name = "debian-gnu",
    .distro = DISTRO_DEBIAN,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .spn = SPN_GNU,
    .lanes = { LANE_GCC },
  },
  {
    .name = "debian-bare",
    .distro = DISTRO_DEBIAN,
    .spn = SPN_GNU,
  },
  {
    .name = "debian-zig",
    .distro = DISTRO_DEBIAN,
    .check = LANE_ZIG,
    .lanes = { LANE_ZIG },
  },
  {
    .name = "ubuntu20",
    .distro = DISTRO_UBUNTU20,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .spn = SPN_GNU,
    .lanes = { LANE_GCC },
  },
  {
    .name = "ubuntu19",
    .distro = DISTRO_UBUNTU19,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .spn = SPN_GNU,
    .lanes = { LANE_GCC },
  },
  {
    .name = "centos7",
    .distro = DISTRO_CENTOS7,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .spn = SPN_GNU,
    .lanes = { LANE_GCC },
  },
  {
    .name = "alpine",
    .distro = DISTRO_ALPINE,
    .compilers = COMPILER_GCC | COMPILER_GXX,
    .check = LANE_GCC,
    .lanes = { LANE_GCC },
  },
  {
    .name = "alpine-clang",
    .distro = DISTRO_ALPINE,
    .compilers = COMPILER_CLANG,
    .check = LANE_CLANG,
    .lanes = { LANE_CLANG },
  },
  {
    .name = "alpine-zig",
    .distro = DISTRO_ALPINE,
    .check = LANE_ZIG,
    .lanes = { LANE_ZIG },
  },
};

const u32 num_variants = sp_carr_len(variants);

static const c8* packages(distro_t distro, compiler_t compiler) {
  switch (distro) {
    case DISTRO_DEBIAN: {
      switch (compiler) {
        case COMPILER_GCC:   return "gcc binutils libc6-dev";
        case COMPILER_GXX:   return "g++";
        case COMPILER_CLANG: return "clang libclang-rt-dev binutils";
      }
      break;
    }
    case DISTRO_UBUNTU20:
    case DISTRO_UBUNTU19: {
      switch (compiler) {
        case COMPILER_GCC:   return "gcc binutils libc6-dev";
        case COMPILER_GXX:   return "g++";
        case COMPILER_CLANG: break;
      }
      break;
    }
    case DISTRO_CENTOS7: {
      switch (compiler) {
        case COMPILER_GCC:   return "gcc binutils glibc-devel";
        case COMPILER_GXX:   return "gcc-c++";
        case COMPILER_CLANG: break;
      }
      break;
    }
    case DISTRO_ALPINE: {
      switch (compiler) {
        case COMPILER_GCC:   return "gcc musl-dev binutils";
        case COMPILER_GXX:   return "g++";
        case COMPILER_CLANG: return "clang musl-dev gcc binutils";
      }
      break;
    }
  }
  SP_UNREACHABLE_RETURN("");
}

static const c8* label(compiler_t compiler) {
  switch (compiler) {
    case COMPILER_GCC:   return "gcc";
    case COMPILER_GXX:   return "g++";
    case COMPILER_CLANG: return "clang/clang++";
  }
  SP_UNREACHABLE_RETURN("");
}

static void push_cstrs(sp_da(sp_str_t)* out, const c8* const* items, u32 max) {
  sp_for(it, max) {
    if (!items[it]) {
      return;
    }
    sp_da_push(*out, sp_cstr_as_str(items[it]));
  }
}

const c8* lane_name(lane_t lane) {
  return lane_names[lane];
}

lane_t lane_find(sp_str_t name) {
  for (u32 it = LANE_NONE + 1; it < LANE_COUNT; it++) {
    if (sp_str_equal_cstr(name, lane_names[it])) {
      return (lane_t)it;
    }
  }
  return LANE_NONE;
}

const spn_cg_toolchain_t* lane_decl(lane_t lane, const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes) {
  sp_str_t name = sp_cstr_as_str(lane_name(lane));
  const spn_cg_toolchain_t* decl = lanes_find(lanes, name);
  return decl ? decl : lanes_find(builtin, name);
}

verify_t lanes_verify(const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes) {
  for (u32 it = LANE_NONE + 1; it < LANE_COUNT; it++) {
    if (!lane_decl((lane_t)it, builtin, lanes)) {
      return (verify_t) { .kind = VERIFY_LANE_UNDECLARED, .lane = (lane_t)it };
    }
  }
  sp_om_for(lanes->toolchain, it) {
    sp_str_t name = sp_om_at(lanes->toolchain, it)->name;
    if (lane_find(name) == LANE_NONE) {
      return (verify_t) { .kind = VERIFY_LANE_UNLISTED, .name = name };
    }
  }
  return (verify_t) { .kind = VERIFY_OK };
}

const variant_t* variant_find(const c8* name) {
  sp_carr_for(variants, it) {
    if (sp_cstr_equal(variants[it].name, name)) {
      return &variants[it];
    }
  }
  return SP_NULLPTR;
}

bool variant_hosts(const variant_t* variant, lane_t lane) {
  sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
    if (variant->lanes[it] == lane) {
      return true;
    }
  }
  return false;
}

const variant_t* variant_hosting(lane_t lane) {
  sp_carr_for(variants, it) {
    if (variant_hosts(&variants[it], lane)) {
      return &variants[it];
    }
  }
  return SP_NULLPTR;
}

lane_t variant_seed(const variant_t* variant) {
  return variant->check ? variant->check : variant->lanes[0];
}

sp_str_t variant_template(const variant_t* variant) {
  return sp_cstr_as_str(distros[variant->distro].template);
}

sp_str_t variant_packages(sp_mem_t mem, const variant_t* variant) {
  sp_da(sp_str_t) pieces = sp_da_new(mem, sp_str_t);
  sp_da_push(pieces, sp_cstr_as_str(distros[variant->distro].packages));
  sp_carr_for(compilers, it) {
    if (variant->compilers & compilers[it]) {
      sp_da_push(pieces, sp_cstr_as_str(packages(variant->distro, compilers[it])));
    }
  }
  push_cstrs(&pieces, variant->extra, SMOKE_MAX_PACKAGES);
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    sp_assert(variant->distro == DISTRO_DEBIAN);
    push_cstrs(&pieces, sysroots[variant->sysroots[it]].packages, SMOKE_MAX_PACKAGES);
  }
  return sp_str_join_n(mem, pieces, (u32)sp_da_size(pieces), sp_str_lit(" "));
}

sp_str_t sysroot_links_setup(sp_mem_t mem, const sysroot_t* sysroot) {
  sp_da(sp_str_t) steps = sp_da_new(mem, sp_str_t);
  sp_da_push(steps, sp_fmt(mem, "mkdir -p {}", sp_fmt_cstr(sysroot->path)).value);
  sp_carr_for_until(sysroot->links, it, sysroot->links[it].dir) {
    sp_da_push(steps, sp_fmt(mem, "ln -s {} {}/{}", sp_fmt_cstr(sysroot->links[it].from), sp_fmt_cstr(sysroot->path), sp_fmt_cstr(sysroot->links[it].dir)).value);
  }
  return sp_str_join_n(mem, steps, (u32)sp_da_size(steps), sp_str_lit(" && "));
}

sp_str_t sysroot_debs_setup(sp_mem_t mem, const sysroot_t* sysroot) {
  sp_da(sp_str_t) debs = sp_da_new(mem, sp_str_t);
  sp_carr_for_until(sysroot->debs.names, it, sysroot->debs.names[it]) {
    sp_da_push(debs, sp_fmt(mem, "{}:{}", sp_fmt_cstr(sysroot->debs.names[it]), sp_fmt_cstr(sysroot->debs.arch)).value);
  }
  sp_da(sp_str_t) steps = sp_da_new(mem, sp_str_t);
  sp_da_push(steps, sp_fmt(mem, "dpkg --add-architecture {}", sp_fmt_cstr(sysroot->debs.arch)).value);
  sp_da_push(steps, sp_str_lit("apt-get update"));
  sp_da_push(steps, sp_str_lit("cd /tmp"));
  sp_da_push(steps, sp_fmt(mem, "apt-get download {}", sp_fmt_str(sp_str_join_n(mem, debs, (u32)sp_da_size(debs), sp_str_lit(" ")))).value);
  sp_da_push(steps, sp_fmt(mem, "mkdir -p {}", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(steps, sp_fmt(mem, "for d in *.deb; do dpkg -x \"$d\" {}; done", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(steps, sp_str_lit("rm -f *.deb"));
  sp_da_push(steps, sp_str_lit("rm -rf /var/lib/apt/lists/*"));
  return sp_str_join_n(mem, steps, (u32)sp_da_size(steps), sp_str_lit(" && "));
}

sp_str_t variant_summary(sp_mem_t mem, const variant_t* variant) {
  sp_da(sp_str_t) pieces = sp_da_new(mem, sp_str_t);
  sp_da_push(pieces, sp_cstr_as_str(distros[variant->distro].label));

  if (variant->compilers) {
    sp_carr_for(compilers, it) {
      if (variant->compilers & compilers[it]) {
        sp_da_push(pieces, sp_cstr_as_str(label(compilers[it])));
      }
    }
  }
  else {
    sp_da_push(pieces, sp_cstr_as_str(variant->check ? "no system compilers" : "no compilers"));
  }

  if (variant->check) {
    sp_da_push(pieces, sp_fmt(mem, "check {}", sp_fmt_cstr(lane_name(variant->check))).value);
  }
  if (variant->spn == SPN_GNU) {
    sp_da_push(pieces, sp_str_lit("dynamic spn"));
  }
  if (variant->sysroots[0]) {
    sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
    sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
      sp_da_push(names, sp_cstr_as_str(sysroots[variant->sysroots[it]].name));
    }
    sp_da_push(pieces, sp_fmt(mem, "sysroots {}", sp_fmt_str(sp_str_join_n(mem, names, (u32)sp_da_size(names), sp_str_lit(" ")))).value);
  }
  if (variant->lanes[0]) {
    sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
    sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
      sp_da_push(names, sp_cstr_as_str(lane_name(variant->lanes[it])));
    }
    sp_da_push(pieces, sp_fmt(mem, "lanes {}", sp_fmt_str(sp_str_join_n(mem, names, (u32)sp_da_size(names), sp_str_lit(" ")))).value);
  }
  return sp_str_join_n(mem, pieces, (u32)sp_da_size(pieces), sp_str_lit(", "));
}

static bool provides(const variant_t* variant, sp_str_t sysroot) {
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    if (sp_str_starts_with(sysroot, sp_cstr_as_str(sysroots[variant->sysroots[it]].path))) {
      return true;
    }
  }
  return false;
}

verify_t variant_verify(const variant_t* variant, const spn_cg_toolchains_t* builtin, const spn_cg_toolchains_t* lanes) {
  sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
    const spn_cg_toolchain_t* lane = lane_decl(variant->lanes[it], builtin, lanes);
    sp_assert(lane);
    sp_da_for(lane->target, target) {
      sp_str_t sysroot = lane->target[target].sysroot;
      if (!sp_str_starts_with(sysroot, sp_str_lit("/")) || provides(variant, sysroot)) {
        continue;
      }
      return (verify_t) { .kind = VERIFY_SYSROOT_UNPROVIDED, .variant = variant, .lane = variant->lanes[it], .sysroot = sysroot };
    }
  }
  return (verify_t) { .kind = VERIFY_OK };
}
