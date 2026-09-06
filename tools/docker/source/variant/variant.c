#include "variant.h"

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
    .lanes = { "llvm", "clang", "gcc", "gcc-lld" },
  },
  {
    .name = "debian-cross",
    .distro = DISTRO_DEBIAN,
    .extra = "gcc-aarch64-linux-gnu",
    .lanes = { "aarch64-gnu" },
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
    .extra = "gcc-mingw-w64-x86-64",
    .lanes = { "clang-mingw" },
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

const c8* variant_template(const variant_t* variant) {
  return distros[variant->distro].template;
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

sp_str_t variant_packages(sp_mem_t mem, const variant_t* variant) {
  const c8* pieces [2 + sp_carr_len(compilers)];
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
  return sp_str_join_cstr_n(mem, pieces, count, sp_str_lit(" "));
}

sp_str_t variant_summary(sp_mem_t mem, const variant_t* variant) {
  const c8* pieces [4 + sp_carr_len(compilers)];
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
  if (variant->lanes[0]) {
    u32 num_lanes = 0;
    sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
      num_lanes++;
    }
    pieces[count++] = sp_str_to_cstr(mem, sp_fmt(mem, "lanes {}", sp_fmt_str(sp_str_join_cstr_n(mem, variant->lanes, num_lanes, sp_str_lit(" ")))).value);
  }
  return sp_str_join_cstr_n(mem, pieces, count, sp_str_lit(", "));
}
