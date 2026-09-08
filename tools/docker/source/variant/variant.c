#include "variant.h"

#include "yyjson.h"

#if defined(SP_ARM64)
  #define SMOKE_HOST "aarch64-linux"
#else
  #define SMOKE_HOST "x86_64-linux"
#endif

typedef struct {
  const c8* template;
  const c8* packages;
  const c8* label;
} distro_t;

static const distro_t distros [] = {
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

static const sysroot_t sysroots [] = {
  [SYSROOT_MUSL] = {
    .name = "musl",
    .path = "/sysroot/musl",
    .packages = "musl musl-dev musl-tools",
    .kind = INSTALL_LINKS,
    .links = {
      { "include", "/usr/include/x86_64-linux-musl" },
      { "lib", "/usr/lib/x86_64-linux-musl" },
    },
  },
  [SYSROOT_WASI] = {
    .name = "wasi",
    .path = "/usr",
    .packages = "wasi-libc libclang-rt-dev-wasm32",
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
    .kind = INSTALL_TARBALL,
    .tarball = { .lane = "wasi-sdk" },
  },
};

static const compiler_t compilers [] = { COMPILER_GCC, COMPILER_GXX, COMPILER_CLANG };

const variant_t variants [] = {
  {
    .name = "debian-gcc",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .extra = "lld",
    .lanes = { "gcc", "gcc-lld" },
  },
  {
    .name = "debian-clang",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .lanes = { "clang" },
  },
  {
    .name = "debian-llvm",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_GCC | COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .extra = "lld llvm",
    .lanes = { "llvm", "clang", "gcc", "gcc-lld", "clang-bare" },
  },
  {
    .name = "debian-cross",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .extra = "gcc-aarch64-linux-gnu libc6-dev-arm64-cross",
    .lanes = { "aarch64-gnu", "clang-cross" },
  },
  {
    .name = "debian-mingw",
    .distro = DISTRO_DEBIAN,
    .extra = "gcc-mingw-w64-x86-64",
    .lanes = { "mingw-gnu" },
  },
  {
    .name = "debian-mingw-clang",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .extra = "gcc-mingw-w64-x86-64 lld",
    .lanes = { "clang-mingw", "clang-mingw-lld" },
  },
  {
    .name = "debian-musl",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_GCC | COMPILER_CLANG,
    .toolchain = TOOLCHAIN_GCC,
    .sysroots = { SYSROOT_MUSL },
    .lanes = { "musl-gcc", "clang-musl" },
  },
  {
    .name = "debian-wasi",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .extra = "lld llvm",
    .sysroots = { SYSROOT_WASI },
    .lanes = { "clang-wasi" },
  },
  {
    .name = "debian-wasi-sdk",
    .distro = DISTRO_DEBIAN,
    .sysroots = { SYSROOT_WASI_SDK },
    .lanes = { "wasi-sdk", "wasi-sdk-local" },
  },
  {
    .name = "debian-sysroot",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .extra = "lld llvm",
    .sysroots = { SYSROOT_ARM64 },
    .lanes = { "clang-sysroot" },
  },
  {
    .name = "debian-cc-only",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_GCC,
    .toolchain = TOOLCHAIN_GCC,
    .lanes = { "gcc" },
  },
  {
    .name = "debian-gnu",
    .distro = DISTRO_DEBIAN,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .spn = SPN_GNU,
    .lanes = { "gcc" },
  },
  {
    .name = "debian-bare",
    .distro = DISTRO_DEBIAN,
    .spn = SPN_GNU,
  },
  {
    .name = "debian-zig",
    .distro = DISTRO_DEBIAN,
    .toolchain = TOOLCHAIN_ZIG,
    .lanes = { "zig" },
  },
  {
    .name = "ubuntu20",
    .distro = DISTRO_UBUNTU20,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .spn = SPN_GNU,
    .lanes = { "gcc" },
  },
  {
    .name = "ubuntu19",
    .distro = DISTRO_UBUNTU19,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .spn = SPN_GNU,
    .lanes = { "gcc" },
  },
  {
    .name = "centos7",
    .distro = DISTRO_CENTOS7,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .spn = SPN_GNU,
    .lanes = { "gcc" },
  },
  {
    .name = "alpine",
    .distro = DISTRO_ALPINE,
    .installed = COMPILER_GCC | COMPILER_GXX,
    .toolchain = TOOLCHAIN_GCC,
    .lanes = { "gcc" },
  },
  {
    .name = "alpine-clang",
    .distro = DISTRO_ALPINE,
    .installed = COMPILER_CLANG,
    .toolchain = TOOLCHAIN_CLANG,
    .lanes = { "clang" },
  },
  {
    .name = "alpine-zig",
    .distro = DISTRO_ALPINE,
    .toolchain = TOOLCHAIN_ZIG,
    .lanes = { "zig" },
  },
};

const u32 num_variants = sp_carr_len(variants);

static const c8* packages(distro_kind_t distro, compiler_t compiler) {
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

static bool provisioned(const variant_t* variant) {
  switch (variant->toolchain) {
    case TOOLCHAIN_NONE:  return false;
    case TOOLCHAIN_GCC:   return !(variant->installed & COMPILER_GCC);
    case TOOLCHAIN_CLANG: return !(variant->installed & COMPILER_CLANG);
    case TOOLCHAIN_ZIG:   return true;
  }
  SP_UNREACHABLE_RETURN(false);
}

const variant_t* variant_find(const c8* name) {
  sp_carr_for(variants, it) {
    if (sp_cstr_equal(variants[it].name, name)) {
      return &variants[it];
    }
  }
  return SP_NULLPTR;
}

bool variant_hosts(const variant_t* variant, const c8* lane) {
  sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
    if (sp_cstr_equal(variant->lanes[it], lane)) {
      return true;
    }
  }
  return false;
}

const variant_t* variant_hosting(const c8* lane) {
  sp_carr_for(variants, it) {
    if (variant_hosts(&variants[it], lane)) {
      return &variants[it];
    }
  }
  return SP_NULLPTR;
}

sp_str_t get_template_name(const variant_t* variant) {
  return sp_cstr_as_str(distros[variant->distro].template);
}

const c8* toolchain_name(toolchain_t toolchain) {
  switch (toolchain) {
    case TOOLCHAIN_NONE:  break;
    case TOOLCHAIN_GCC:   return "gcc";
    case TOOLCHAIN_CLANG: return "clang";
    case TOOLCHAIN_ZIG:   return "zig";
  }
  SP_UNREACHABLE_RETURN("");
}

const c8* variant_seed(const variant_t* variant) {
  if (variant->toolchain != TOOLCHAIN_NONE) {
    return toolchain_name(variant->toolchain);
  }
  return variant->lanes[0];
}

sp_str_t get_variant_packages(sp_mem_t mem, const variant_t* variant) {
  const c8* pieces [2 + sp_carr_len(compilers) + SMOKE_MAX_SYSROOTS];
  u32 count = 0;
  pieces[count++] = distros[variant->distro].packages;
  sp_carr_for(compilers, it) {
    if (variant->installed & compilers[it]) {
      pieces[count++] = packages(variant->distro, compilers[it]);
    }
  }
  if (variant->extra) {
    pieces[count++] = variant->extra;
  }
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    sp_assert(variant->distro == DISTRO_DEBIAN);
    const sysroot_t* sysroot = &sysroots[variant->sysroots[it]];
    if (sysroot->packages) {
      pieces[count++] = sysroot->packages;
    }
  }
  return sp_str_join_cstr_n(mem, pieces, count, sp_str_lit(" "));
}

static void push_links_setup(sp_mem_t mem, sp_da(sp_str_t)* steps, const sysroot_t* sysroot) {
  sp_da_push(*steps, sp_fmt(mem, "mkdir -p {}", sp_fmt_cstr(sysroot->path)).value);
  sp_carr_for_until(sysroot->links, it, sysroot->links[it].dir) {
    sp_da_push(*steps, sp_fmt(mem, "ln -s {} {}/{}", sp_fmt_cstr(sysroot->links[it].from), sp_fmt_cstr(sysroot->path), sp_fmt_cstr(sysroot->links[it].dir)).value);
  }
}

static void push_debs_setup(sp_mem_t mem, sp_da(sp_str_t)* steps, const sysroot_t* sysroot) {
  const c8* debs [SMOKE_MAX_DEBS];
  u32 count = 0;
  sp_carr_for_until(sysroot->debs.names, it, sysroot->debs.names[it]) {
    debs[count++] = sp_str_to_cstr(mem, sp_fmt(mem, "{}:{}", sp_fmt_cstr(sysroot->debs.names[it]), sp_fmt_cstr(sysroot->debs.arch)).value);
  }
  sp_da_push(*steps, sp_fmt(mem, "dpkg --add-architecture {}", sp_fmt_cstr(sysroot->debs.arch)).value);
  sp_da_push(*steps, sp_str_lit("apt-get update"));
  sp_da_push(*steps, sp_str_lit("cd /tmp"));
  sp_da_push(*steps, sp_fmt(mem, "apt-get download {}", sp_fmt_str(sp_str_join_cstr_n(mem, debs, count, sp_str_lit(" ")))).value);
  sp_da_push(*steps, sp_fmt(mem, "mkdir -p {}", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(*steps, sp_fmt(mem, "for d in *.deb; do dpkg -x \"$d\" {}; done", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(*steps, sp_str_lit("rm -f *.deb"));
  sp_da_push(*steps, sp_str_lit("rm -rf /var/lib/apt/lists/*"));
}

yyjson_val* lane_find(yyjson_val* lanes, const c8* name) {
  size_t idx, max;
  yyjson_val* lane;
  yyjson_arr_foreach(lanes, idx, max, lane) {
    if (sp_cstr_equal(yyjson_get_str(yyjson_obj_get(lane, "name")), name)) {
      return lane;
    }
  }
  return SP_NULLPTR;
}

static yyjson_val* lane_artifact(yyjson_val* lanes, const c8* name) {
  return yyjson_obj_get(yyjson_obj_get(lane_find(lanes, name), "host"), SMOKE_HOST);
}

static void push_tarball_setup(sp_mem_t mem, sp_da(sp_str_t)* steps, const sysroot_t* sysroot, yyjson_val* lanes) {
  yyjson_val* artifact = lane_artifact(lanes, sysroot->tarball.lane);
  sp_assert(artifact);
  const c8* url = yyjson_get_str(yyjson_obj_get(artifact, "url"));
  const c8* sha256 = yyjson_get_str(yyjson_obj_get(artifact, "sha256"));
  sp_da_push(*steps, sp_str_lit("cd /tmp"));
  sp_da_push(*steps, sp_fmt(mem, "curl -fsSL -o tarball {}", sp_fmt_cstr(url)).value);
  sp_da_push(*steps, sp_fmt(mem, "echo \"{}  tarball\" | sha256sum -c -", sp_fmt_cstr(sha256)).value);
  sp_da_push(*steps, sp_fmt(mem, "mkdir -p {}", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(*steps, sp_fmt(mem, "tar xf tarball -C {} --strip-components=1", sp_fmt_cstr(sysroot->path)).value);
  sp_da_push(*steps, sp_str_lit("rm -f tarball"));
}

static void push_sysroot_setup(sp_mem_t mem, sp_da(sp_str_t)* steps, const sysroot_t* sysroot, yyjson_val* lanes) {
  switch (sysroot->kind) {
    case INSTALL_PACKAGES: return;
    case INSTALL_LINKS:    push_links_setup(mem, steps, sysroot); return;
    case INSTALL_DEBS:     push_debs_setup(mem, steps, sysroot); return;
    case INSTALL_TARBALL:  push_tarball_setup(mem, steps, sysroot, lanes); return;
  }
  SP_UNREACHABLE();
}

sp_str_t get_variant_setup(sp_mem_t mem, const variant_t* variant, yyjson_val* lanes) {
  sp_da(sp_str_t) steps = sp_da_new(mem, sp_str_t);
  sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
    push_sysroot_setup(mem, &steps, &sysroots[variant->sysroots[it]], lanes);
  }
  return sp_str_join_n(mem, steps, (u32)sp_da_size(steps), sp_str_lit(" && "));
}

sp_str_t variant_summary(sp_mem_t mem, const variant_t* variant) {
  const c8* pieces [5 + sp_carr_len(compilers)];
  u32 count = 0;
  pieces[count++] = distros[variant->distro].label;

  if (variant->installed) {
    sp_carr_for(compilers, it) {
      if (variant->installed & compilers[it]) {
        pieces[count++] = label(compilers[it]);
      }
    }
  }
  else {
    pieces[count++] = variant->toolchain == TOOLCHAIN_NONE ? "no compilers" : "no system compilers";
  }

  if (provisioned(variant)) {
    pieces[count++] = sp_str_to_cstr(mem, sp_fmt(mem, "provisioned {}", sp_fmt_cstr(toolchain_name(variant->toolchain))).value);
  }
  if (variant->spn == SPN_GNU) {
    pieces[count++] = "dynamic spn";
  }
  if (variant->sysroots[0]) {
    const c8* names [SMOKE_MAX_SYSROOTS];
    u32 num_sysroots = 0;
    sp_carr_for_until(variant->sysroots, it, variant->sysroots[it]) {
      names[num_sysroots++] = sysroots[variant->sysroots[it]].name;
    }
    pieces[count++] = sp_str_to_cstr(mem, sp_fmt(mem, "sysroots {}", sp_fmt_str(sp_str_join_cstr_n(mem, names, num_sysroots, sp_str_lit(" ")))).value);
  }
  if (variant->lanes[0]) {
    u32 num_lanes = 0;
    sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
      num_lanes++;
    }
    pieces[count++] = sp_str_to_cstr(mem, sp_fmt(mem, "lanes {}", sp_fmt_str(sp_str_join_cstr_n(mem, variant->lanes, num_lanes, sp_str_lit(" ")))).value);
  }
  return sp_str_join_cstr_n(mem, pieces, count, sp_str_lit(", "));
}
