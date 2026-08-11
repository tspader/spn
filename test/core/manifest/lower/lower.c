#include "spn_test.h"

#include "intern/intern.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "manifest.gen.h"
#include "target/types.h"
#include "profile/types.h"
#include "index/types.h"
#include "toolchain/types.h"
#include "toml/loader.h"
#include "semver/compare.h"
#include "when/when.h"

////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* value;
  const c8* when;
  spn_tree_t tree;
  bool tree_none;
} gated_t;

typedef struct {
  const c8* name;
  spn_linkage_set_t linkages;
  bool no_link;
  gated_t source [4];
  gated_t headers [4];
  gated_t include [4];
  gated_t define [4];
  gated_t flags [4];
  gated_t system_deps [4];
  gated_t deps [4];
  spn_cxx_options_t cxx;
} target_t;

typedef struct {
  const c8* name;
  spn_pkg_source_t source;
  const c8* file;
  u8 build;
  u8 test;
  u8 private;
  const c8* when;
  const c8* options;
} dep_t;

typedef struct {
  const c8* when;
  const c8* value;
} option_default_t;

typedef struct {
  const c8* name;
  spn_option_type_t type;
  bool additive;
  bool public;
  const c8* define;
  const c8* values [4];
  option_default_t defaults [4];
} option_t;

typedef struct {
  const c8* name;
  bool remote;
  const c8* url;
  const c8* sha256;
  const c8* mirrors;
  const c8* compiler;
  const c8* args [8];
  const c8* linker;
  const c8* archiver;
  const c8* cxx;
  const c8* cxx_args [8];
  spn_cc_driver_t driver;
  spn_triple_t targets [4];
} toolchain_t;

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  bool sanitizers_set;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  const c8* options;
} profile_t;

typedef struct {
  const c8* name;
  const c8* url;
  const c8* path;
  spn_index_protocol_t protocol;
  spn_index_kind_t kind;
} index_t;

typedef struct {
  const c8* key;
  spn_linkage_t kind;
  const c8* options;
  bool defaults_declined;
} config_t;

typedef struct {
  spn_err_t code;
  const c8* path;
} issue_t;

typedef struct {
  const c8* name;
  const c8* files [4];
  bool hashed;
} patch_t;

typedef struct {
  const c8* name;
  const c8* manifest;
  const c8* pkg_name;
  const c8* namespace;
  const c8* qualified;
  const c8* url;
  const c8* repo;
  const c8* author;
  const c8* maintainer;
  spn_semver_t version;
  const c8* commit;
  gated_t include [4];
  gated_t build_source [4];
  gated_t build_include [4];
  const c8* define [8];
  gated_t system_deps [8];
  issue_t issues [7];
  target_t libs [8];
  target_t exes [8];
  target_t scripts [8];
  target_t tests [8];
  target_t examples [8];
  dep_t deps [8];
  toolchain_t toolchains [4];
  profile_t profiles [4];
  index_t indexes [4];
  config_t config [8];
  option_t options [4];
  patch_t patches [4];
  bool hash_patches;
  bool reject_patches;
} test_t;

///////////
// CASES //
///////////
static const test_t tests [] = {
  {
    .name = "lib_shared",
    .manifest = "shared_lib",
    .libs = {
      { .name = "t", .linkages = { .shared = true } }
    }
  },
  {
    .name = "lib_object",
    .manifest = "object_lib",
    .libs = {
      { .name = "t", .linkages = { .object = true } }
    }
  },
  {
    .name = "lib_source_static",
    .manifest = "source_static",
    .libs = {
      { .name = "t", .linkages = { .source = true, .static_lib = true } }
    }
  },
  {
    .name = "lib_no_link",
    .manifest = "no_link_lib",
    .libs = {
      { .name = "t", .linkages = { .static_lib = true }, .no_link = true }
    }
  },
  {
    .name = "lib_link_true",
    .manifest = "link_true_lib",
    .libs = {
      { .name = "t", .linkages = { .static_lib = true } }
    }
  },
  {
    .name = "lib_all_fields",
    .manifest = "lib_all_fields",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "main.c" } },
        .headers = { { "header.h" } },
        .include = { { "include/dir" } },
        .define = { { "SPUM" } },
        .flags = { { "-flag" } },
        .deps = { { "spum" } },
      }
    }
  },
  {
    .name = "exe",
    .manifest = "exe",
    .exes = {
      { .name = "t" }
    }
  },
  {
    .name = "script",
    .manifest = "script",
    .scripts = {
      { .name = "t" }
    }
  },
  {
    .name = "test",
    .manifest = "target_test",
    .tests = {
      { .name = "t" }
    }
  },
  {
    .name = "example",
    .manifest = "target_example",
    .examples = {
      { .name = "t" }
    }
  },
  {
    .name = "dep_index",
    .manifest = "dep_index",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX }
    }
  },
  {
    .name = "dep_file",
    .manifest = "dep_file",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "/abs/foo/spn.toml" }
    }
  },
  {
    .name = "dep_file_relative",
    .manifest = "dep_file_relative",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "vendor/foo/spn.toml" }
    }
  },
  {
    .name = "dep_test",
    .manifest = "dep_test",
    .deps = {
      { .name = "core/bar", .source = SPN_PKG_SOURCE_INDEX, .test = 1 }
    }
  },
  {
    .name = "dep_build",
    .manifest = "dep_build",
    .deps = {
      { .name = "core/baz", .source = SPN_PKG_SOURCE_INDEX, .build = 1 }
    }
  },
  {
    .name = "dep_namespaced",
    .manifest = "dep_namespaced",
    .deps = {
      { .name = "ns/q", .source = SPN_PKG_SOURCE_INDEX }
    }
  },
  {
    .name = "dep_private",
    .manifest = "dep_private",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX, .private = 1 }
    }
  },
  // The same name under two kinds must survive as two requests, not collapse
  {
    .name = "dep_dual_kind",
    .manifest = "dep_dual_kind",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX },
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX, .build = 1 }
    }
  },
  {
    .name = "cxx_lib",
    .manifest = "cxx_lib",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "spum.cpp" } },
        .cxx = { .standard = SPN_CXX14, .no_exceptions = true, .no_rtti = true },
      }
    }
  },
  {
    .name = "cxx_lib_defaults",
    .manifest = "cxx_lib_defaults",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "spum.cpp" } },
      }
    }
  },
  {
    .name = "validate_cxx_bad_standard",
    .manifest = "validate_cxx_bad_standard",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "lib[0].cxx.standard" }
    }
  },
  {
    .name = "validate_cxx_source_on_build_script",
    .manifest = "validate_cxx_source_on_build_script",
    .build_source = { { "tools/build.cpp" } },
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "package.build.source" }
    }
  },
  {
    .name = "toolchain_cxx",
    .manifest = "toolchain_cxx",
    .toolchains = {
      {
        .name = "custom",
        .compiler = "cc",
        .linker = "cc",
        .archiver = "ar",
        .cxx = "g++",
        .cxx_args = { "-pthread" },
        .driver = SPN_CC_DRIVER_GCC,
      },
    },
  },
  {
    .name = "validate_object_mixed",
    .manifest = "validate_object_mixed",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "lib[0].kinds" }
    }
  },
  {
    .name = "validate_link_on_bin",
    .manifest = "validate_link_on_bin",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "bin[0].link" }
    }
  },
  {
    .name = "validate_link_on_script",
    .manifest = "validate_link_on_script",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "script[0].link" }
    }
  },
  {
    .name = "validate_link_on_test",
    .manifest = "validate_link_on_test",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "test[0].link" }
    }
  },
  {
    .name = "validate_toolchain_incomplete",
    .manifest = "toolchain_incomplete",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "toolchain[0].compiler" }
    }
  },
  {
    .name = "validate_toolchain_url_without_sha",
    .manifest = "toolchain_no_sha",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "toolchain[0].sha256" }
    }
  },
  {
    .name = "validate_duplicate_name",
    .manifest = "validate_duplicate_name",
    .issues = {
      { SPN_ERR_CODEGEN_DUPLICATE_KEY, "bin[0]" }
    }
  },
  {
    .name = "validate_upstream_url_only",
    .manifest = "validate_upstream_url_only",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "package.upstream.commit" }
    }
  },
  {
    .name = "validate_upstream_commit_only",
    .manifest = "validate_upstream_commit_only",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "package.upstream.url" }
    }
  },
  {
    .name = "target_tree",
    .manifest = "target_tree",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "a.c" }, { "b.c", .tree = SPN_TREE_MANIFEST }, { "c.c", .tree = SPN_TREE_SOURCE } },
        .headers = { { "a.h" }, { "b.h", .tree = SPN_TREE_MANIFEST } },
        .include = { { "inc", .tree = SPN_TREE_MANIFEST } },
      }
    }
  },
  {
    .name = "validate_tree_invalid",
    .manifest = "validate_tree_invalid",
    .include = { { "inc", .tree_none = true } },
    .build_source = { { "b.c", .tree_none = true } },
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "lib[0].source[0].tree" },
      { SPN_ERR_CODEGEN_INVALID, "lib[0].headers[0].tree" },
      { SPN_ERR_CODEGEN_INVALID, "package.include[0].tree" },
      { SPN_ERR_CODEGEN_INVALID, "package.build.source[0].tree" },
      { SPN_ERR_CODEGEN_INVALID, "package.configure.include[0].tree" },
    }
  },
  {
    .name = "script_tree",
    .manifest = "script_tree",
    .build_source = { { "b.c", .tree = SPN_TREE_MANIFEST }, { "c.c", .when = "os = \"linux\"" } },
    .build_include = { { "inc", .tree = SPN_TREE_MANIFEST } },
  },
  {
    .name = "validate_metaprogram_when_option",
    .manifest = "validate_metaprogram_when_option",
    .build_source = { { "b.c", .when = "tls = true" } },
    .options = {
      {
        .name = "tls",
        .type = SPN_OPTION_TYPE_BOOL,
      },
    },
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "package.build.source[0].when.tls" },
    }
  },
  {
    .name = "package",
    .manifest = "package",
    .pkg_name = "p",
    .namespace = "ns",
    .qualified = "ns/p",
    .url = "u",
    .repo = "r",
    .author = "a",
    .maintainer = "m",
    .version = spn_semver_lit(1, 2, 3),
    .commit = "abc",
    .include = { { "inc" } },
    .define = { "SPUM" },
    .system_deps = { { "z" } },
  },
  {
    .name = "package_include_tree",
    .manifest = "package_include_tree",
    .include = { { "inc", .tree = SPN_TREE_MANIFEST }, { "src" }, { "gen", .when = "os = \"linux\"" } },
  },
  {
    .name = "qualified_default_namespace",
    .manifest = "qualified_default_namespace",
    .qualified = "core/p",
  },
  {
    .name = "toolchain_inline",
    .manifest = "toolchain_inline",
    .toolchains = {
      {
        .name = "zig",
        .remote = true,
        .url = "https://tc",
        .sha256 = "deadbeef",
        .mirrors = "https://mirrors",
        .compiler = "zig",
        .args = { "cc", "-target", "x86_64-linux-gnu" },
        .linker = "zig",
        .archiver = "ar",
        .driver = SPN_CC_DRIVER_CLANG,
        .targets = { { SPN_ARCH_ARM64, SPN_OS_MACOS } },
      },
    },
  },
  {
    .name = "profile",
    .manifest = "profiles",
    .profiles = {
      {
        .name = "release",
        .toolchain = "zig",
        .linkage = SPN_LIB_KIND_SHARED,
        .standard = SPN_C99,
        .mode = SPN_BUILD_MODE_RELEASE,
        .opt = SPN_OPT_LEVEL_3,
        .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
        .sanitizers_set = true,
        .os = SPN_OS_LINUX,
        .arch = SPN_ARCH_X64,
        .abi = SPN_ABI_GNU,
      },
      {
        .name = "clean",
        .sanitizers_set = true,
      },
    },
  },
  {
    .name = "index",
    .manifest = "indexes",
    .indexes = {
      {
        .name = "main",
        .url = "https://x",
        .protocol = SPN_INDEX_PROTOCOL_HTTP,
        .kind = SPN_INDEX_KIND_WORKSPACE,
      },
      {
        .name = "mirror",
        .url = "https://y",
        .kind = SPN_INDEX_KIND_WORKSPACE,
      },
      {
        .name = "local",
        .path = "index",
        .protocol = SPN_INDEX_PROTOCOL_DIR,
        .kind = SPN_INDEX_KIND_WORKSPACE,
      },
    },
  },
  {
    .name = "validate_index_sources",
    .manifest = "index_sources",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "index[0].url" },
      { SPN_ERR_CODEGEN_MISSING_KEY, "index[1].url" },
      { SPN_ERR_CODEGEN_INVALID, "index[2].path" },
      { SPN_ERR_CODEGEN_MISSING_KEY, "index[2].url" },
      { SPN_ERR_CODEGEN_INVALID, "index[3].url" },
      { SPN_ERR_CODEGEN_INVALID, "index[3].rev" },
      { SPN_ERR_CODEGEN_MISSING_KEY, "index[3].path" },
    }
  },
  {
    .name = "validate_package_bad_version",
    .manifest = "package_bad_version",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "package.version" }
    }
  },
  {
    .name = "config",
    .manifest = "config",
    .config = {
      { .key = "core/foo", .kind = SPN_LIB_KIND_STATIC }
    },
  },
  {
    .name = "when_dep",
    .manifest = "when_dep",
    .deps = {
      { .name = "core/openssl", .source = SPN_PKG_SOURCE_INDEX, .when = "tls = \"openssl\"" },
      { .name = "core/zstd", .source = SPN_PKG_SOURCE_INDEX, .when = "zstd = true, os != \"wasi\"" },
    },
  },
  {
    .name = "when_new_facts",
    .manifest = "when_new_facts",
    .deps = {
      { .name = "core/fast", .source = SPN_PKG_SOURCE_INDEX, .when = "opt = \"3\"" },
      { .name = "core/asan", .source = SPN_PKG_SOURCE_INDEX, .when = "sanitize_address = true" },
    },
  },
  {
    .name = "validate_when_bad_new_facts",
    .manifest = "validate_when_bad_new_facts",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "deps.package[0].when.opt" },
      { SPN_ERR_CODEGEN_INVALID, "deps.package[1].when.sanitize_address" },
    },
  },
  {
    .name = "validate_profile_bad_opt",
    .manifest = "validate_profile_bad_opt",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "profile[0].opt" },
    },
  },
  {
    .name = "validate_profile_sanitize_conflict",
    .manifest = "validate_profile_sanitize_conflict",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "profile[0].sanitize" },
    },
  },
  {
    .name = "validate_profile_bad_sanitizer",
    .manifest = "validate_profile_bad_sanitizer",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "profile.dev.sanitize[0]" },
    },
  },
  {
    .name = "options",
    .manifest = "options",
    .options = {
      {
        .name = "tls",
        .type = SPN_OPTION_TYPE_ENUM,
        .values = { "schannel", "openssl", "off" },
        .defaults = {
          { .when = "os = \"windows\"", .value = "\"schannel\"" },
          { .when = "os != \"wasi\"", .value = "\"openssl\"" },
          { .value = "\"off\"" },
        },
      },
      {
        .name = "zstd",
        .type = SPN_OPTION_TYPE_BOOL,
        .defaults = {
          { .value = "false" },
        },
      },
    },
  },
  {
    .name = "target_when_gated",
    .manifest = "target_when",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "a.c" }, { "b.c", "os = \"linux\"" } },
        .define = { { "X" }, { "Y", "os = \"windows\"" } },
        .flags = { { "-g", "mode = \"debug\"" } },
        .system_deps = { { "ws2_32", "os = \"windows\"" } },
      },
    },
  },
  {
    .name = "option_extras",
    .manifest = "option_extras",
    .options = {
      {
        .name = "audio",
        .type = SPN_OPTION_TYPE_BOOL,
        .additive = true,
        .public = true,
        .define = "HAS_AUDIO",
        .defaults = { { .value = "false" } },
      },
    },
    .deps = {
      { .name = "core/codec", .source = SPN_PKG_SOURCE_INDEX, .options = "audio = true, backend != \"vk\"" },
    },
    .profiles = {
      { .name = "full", .options = "audio = true" },
    },
    .config = {
      { .key = "codec", .options = "audio = true", .defaults_declined = true },
    },
  },
  {
    .name = "package_system_deps_gated",
    .manifest = "package_system_deps_gated",
    .system_deps = { { "m" }, { "ws2_32", "os = \"windows\"" } },
  },
  {
    .name = "validate_option_define_on_enum",
    .manifest = "validate_option_define_on_enum",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].define" }
    },
  },
  {
    .name = "validate_config_negated_option",
    .manifest = "validate_config_negated_option",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "config[0].options.ssl" }
    },
  },
  {
    .name = "validate_profile_unknown_option",
    .manifest = "validate_profile_unknown_option",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "profile[0].options.nosuch" }
    },
  },
  {
    .name = "validate_when_unknown_key",
    .manifest = "validate_when_unknown_key",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "deps.package[0].when.simd" }
    },
  },
  {
    .name = "validate_when_bad_fact",
    .manifest = "validate_when_bad_fact",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "lib[0].source[0].when.os" },
      { SPN_ERR_CODEGEN_INVALID, "lib[0].source[1].when.mode" },
      { SPN_ERR_CODEGEN_INVALID, "lib[0].source[2].when.abi" }
    },
  },
  {
    .name = "validate_when_kind_mismatch",
    .manifest = "validate_when_kind_mismatch",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "deps.package[0].when.zstd" }
    },
  },
  {
    .name = "validate_when_bad_option_value",
    .manifest = "validate_when_bad_option_value",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "deps.package[0].when.tls" }
    },
  },
  {
    .name = "validate_when_in_default",
    .manifest = "validate_when_in_default",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].default[0].when.simd" }
    },
  },
  {
    .name = "validate_option_bad_type",
    .manifest = "validate_option_bad_type",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].type" }
    },
  },
  {
    .name = "validate_option_enum_no_values",
    .manifest = "validate_option_enum_no_values",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "options[0].values" }
    },
  },
  {
    .name = "validate_option_default_kind",
    .manifest = "validate_option_default_kind",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].default[0].value" }
    },
  },
  {
    .name = "validate_option_default_not_in_values",
    .manifest = "validate_option_default_not_in_values",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].default[0].value" }
    },
  },
  {
    .name = "validate_option_default_bad_index",
    .manifest = "validate_option_default_bad_index",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].default[1].value" }
    },
  },
  {
    .name = "validate_option_shadows_fact",
    .manifest = "validate_option_shadows_fact",
    .issues = {
      { SPN_ERR_CODEGEN_DUPLICATE_KEY, "options[0]" }
    },
  },
  {
    .name = "validate_option_bool_values",
    .manifest = "validate_option_bool_values",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options[0].values" }
    },
  },
  {
    .name = "patch_table",
    .manifest = "patch_table",
    .patches = {
      { .name = "ns/q", .files = { "patches/a.patch", "patches/b.patch" } },
    },
  },
  {
    .name = "patch_default_namespace",
    .manifest = "patch_default_namespace",
    .patches = {
      { .name = "core/q", .files = { "patches/a.patch" } },
    },
  },
  {
    .name = "patch_hashes",
    .manifest = "patch_table",
    .hash_patches = true,
    .patches = {
      { .name = "ns/q", .files = { "patches/a.patch", "patches/b.patch" }, .hashed = true },
    },
  },
  {
    .name = "patch_hash_missing_file",
    .manifest = "patch_missing_file",
    .hash_patches = true,
    .issues = {
      { SPN_ERR_CODEGEN_FILE_MISSING, "patch.core/q[0].files" },
    },
  },
  {
    .name = "patch_hash_missing_files_in_two_entries",
    .manifest = "patch_missing_two",
    .hash_patches = true,
    .issues = {
      { SPN_ERR_CODEGEN_FILE_MISSING, "patch.core/q[0].files" },
      { SPN_ERR_CODEGEN_FILE_MISSING, "patch.core/r[1].files" },
    },
  },
  {
    .name = "patch_duplicate_canonical_name",
    .manifest = "patch_duplicate",
    .issues = {
      { SPN_ERR_CODEGEN_DUPLICATE_KEY, "patch" },
    },
    .patches = {
      { .name = "core/q", .files = { "patches/a.patch" } },
    },
  },
  {
    .name = "patch_duplicate_after_empty_files",
    .manifest = "patch_duplicate_empty",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "patch.q.files" },
      { SPN_ERR_CODEGEN_DUPLICATE_KEY, "patch" },
    },
  },
  {
    .name = "validate_patch_files_empty",
    .manifest = "validate_patch_files_empty",
    .issues = {
      { SPN_ERR_CODEGEN_MISSING_KEY, "patch.q.files" },
    },
  },
  {
    .name = "patch_rejected_outside_root",
    .manifest = "patch_table",
    .reject_patches = true,
    .issues = {
      { SPN_ERR_CODEGEN_ROOT_ONLY, "patch" },
    },
  },
  {
    .name = "validate_option_default_unknown_key",
    .manifest = "validate_option_default_unknown_key",
    .issues = {
      { SPN_ERR_CODEGEN_INVALID, "options.tls.default[0].whne" }
    },
  },
};

//////////////
// EXECUTOR //
//////////////
static sp_err_t check_gated_list(sp_test_t* t, spn_gated_list_t actual, const gated_t* expected, u32 n) {
  sp_must_eq(t, n, (u32)sp_da_size(actual));
  sp_for(i, n) {
    sp_expect_str_eq_c(t, actual[i].value, expected[i].value);
    sp_expect_str_eq_c(t, spn_when_to_str(sp_test_arena(t), &actual[i].when), expected[i].when ? expected[i].when : "always");
  }
  return SP_OK;
}

static sp_err_t check_gated_path_list(sp_test_t* t, spn_gated_path_list_t actual, const gated_t* expected, u32 n) {
  sp_must_eq(t, n, (u32)sp_da_size(actual));
  sp_for(it, n) {
    spn_tree_t tree = expected[it].tree_none ? SPN_TREE_NONE : expected[it].tree ? expected[it].tree : SPN_TREE_SOURCE;
    sp_expect_str_eq_c(t, actual[it].path, expected[it].value);
    sp_expect_eq(t, (u32)tree, (u32)actual[it].tree);
    sp_expect_str_eq_c(t, spn_when_to_str(sp_test_arena(t), &actual[it].when), expected[it].when ? expected[it].when : "always");
  }
  return SP_OK;
}

#define check_gated(t, actual, expected) do { \
  u32 num_gated = 0; \
  sp_carr_detect_len(expected, num_gated, (expected)[num_gated].value); \
  sp_try(check_gated_list(t, actual, expected, num_gated)); \
} while (0)

#define check_gated_paths(t, actual, expected) do { \
  u32 num_gated = 0; \
  sp_carr_detect_len(expected, num_gated, (expected)[num_gated].value); \
  sp_try(check_gated_path_list(t, actual, expected, num_gated)); \
} while (0)

static sp_err_t check_targets(sp_test_t* t, spn_target_map_t om, const target_t* arr, u32 n, spn_target_kind_t kind) {
  for (u32 i = 0; i < n; i++) {
    if (!arr[i].name) break;
    spn_target_info_t* info = sp_str_om_get(om, sp_str_view(arr[i].name));
    sp_must(t, info);
    sp_expect_eq(t, (u32)kind, (u32)info->kind);
    sp_expect_eq(t, arr[i].linkages.source, info->linkages.source);
    sp_expect_eq(t, arr[i].linkages.shared, info->linkages.shared);
    sp_expect_eq(t, arr[i].linkages.static_lib, info->linkages.static_lib);
    sp_expect_eq(t, arr[i].linkages.object, info->linkages.object);
    sp_expect_eq(t, arr[i].no_link, info->no_link);
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->source));
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->include));
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->define));
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->flags));
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->system_deps));
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->deps));
    check_gated_paths(t, info->gated.source, arr[i].source);
    sp_expect_eq(t, (u32)0, (u32)sp_da_size(info->headers));
    check_gated_paths(t, info->gated.headers, arr[i].headers);
    check_gated_paths(t, info->gated.include, arr[i].include);
    check_gated(t, info->gated.define, arr[i].define);
    check_gated(t, info->gated.flags, arr[i].flags);
    check_gated(t, info->gated.system_deps, arr[i].system_deps);
    check_gated(t, info->gated.deps, arr[i].deps);
    sp_expect_eq(t, (u32)arr[i].cxx.standard, (u32)info->cxx.standard);
    sp_expect_eq(t, arr[i].cxx.no_exceptions, info->cxx.no_exceptions);
    sp_expect_eq(t, arr[i].cxx.no_rtti, info->cxx.no_rtti);
  }
  return SP_OK;
}

sp_test_each(lower, cases, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_intern_t* interner = sp_intern_new(mem);

  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, mem, interner);

  sp_str_t file = sp_fmt(mem, "{}.toml", sp_fmt_cstr(it->manifest)).value;
  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(MANIFEST_DIR)), file);

  ctx.dir = test_repo_path(mem, sp_str_lit(MANIFEST_DIR));

  spn_cg_manifest_t cg = sp_zero;
  spn_codegen_load(&ctx, path, &cg);

  spn_pkg_info_t pkg = sp_zero;
  spn_pkg_lower(&ctx, &cg, &pkg);

  if (it->hash_patches) {
    spn_pkg_lower_patch_hashes(&ctx, &pkg);
  }
  if (it->reject_patches) {
    spn_pkg_reject_patches(&ctx, &pkg);
  }

  // Issues
  u32 num_issues = 0;
  sp_carr_detect_len(it->issues, num_issues, it->issues[num_issues].code);
  sp_must_eq(t, num_issues, (u32)sp_da_size(ctx.issues));
  sp_for(i, num_issues) {
    sp_expect_eq(t, (u32)it->issues[i].code, (u32)ctx.issues[i].code);
    if (it->issues[i].path) sp_expect_str_eq_c(t, ctx.issues[i].path, it->issues[i].path);
  }

  // Package scalars
  if (it->pkg_name)   sp_expect_str_eq_c(t, pkg.name, it->pkg_name);
  if (it->namespace)  sp_expect_str_eq_c(t, pkg.namespace, it->namespace);
  if (it->qualified)  sp_expect_str_eq_c(t, pkg.qualified, it->qualified);
  if (it->url)        sp_expect_str_eq_c(t, pkg.upstream.url, it->url);
  if (it->repo)       sp_expect_str_eq_c(t, pkg.repo, it->repo);
  if (it->author)     sp_expect_str_eq_c(t, pkg.author, it->author);
  if (it->maintainer) sp_expect_str_eq_c(t, pkg.maintainer, it->maintainer);

  if (!spn_semver_is_empty(it->version)) {
    sp_expect_eq(t, it->version.major, pkg.version.major);
    sp_expect_eq(t, it->version.minor, pkg.version.minor);
    sp_expect_eq(t, it->version.patch, pkg.version.patch);
  }

  if (it->commit) {
    sp_expect_str_eq_c(t, pkg.upstream.commit, it->commit);
  }

  // Package arrays
  sp_must_strs_eq(t, pkg.define, sp_da_size(pkg.define), it->define);
  check_gated(t, pkg.gated.system_deps, it->system_deps);
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(pkg.include));
  check_gated_paths(t, pkg.gated.include, it->include);
  check_gated_paths(t, pkg.build.gated.source, it->build_source);
  check_gated_paths(t, pkg.build.gated.include, it->build_include);

  // Targets
  sp_try(check_targets(t, pkg.libs,     it->libs,     SP_CARR_LEN(it->libs),     SPN_TARGET_KIND_LIB));
  sp_try(check_targets(t, pkg.exes,     it->exes,     SP_CARR_LEN(it->exes),     SPN_TARGET_KIND_EXE));
  sp_try(check_targets(t, pkg.scripts,  it->scripts,  SP_CARR_LEN(it->scripts),  SPN_TARGET_KIND_SCRIPT));
  sp_try(check_targets(t, pkg.tests,    it->tests,    SP_CARR_LEN(it->tests),    SPN_TARGET_KIND_TEST));
  sp_try(check_targets(t, pkg.examples, it->examples, SP_CARR_LEN(it->examples), SPN_TARGET_KIND_EXAMPLE));

  // Deps
  sp_carr_for(it->deps, d) {
    dep_t expected = it->deps[d];
    if (!expected.name) break;

    spn_dep_kind_t kind = expected.build ? SPN_DEP_KIND_BUILD : expected.test ? SPN_DEP_KIND_TEST : SPN_DEP_KIND_PACKAGE;

    spn_requested_dep_t* req = SP_NULLPTR;
    sp_da_for(pkg.deps, j) {
      if (sp_str_equal(pkg.deps[j].qualified, sp_str_view(expected.name)) && pkg.deps[j].kind == kind) {
        req = &pkg.deps[j];
        break;
      }
    }
    sp_must(t, req);
    sp_expect_eq(t, (u32)expected.source, (u32)req->source);
    sp_expect_eq(t, expected.private != 0, req->private);

    if (expected.file) sp_expect(t, sp_str_ends_with(req->file.path, sp_str_view(expected.file)));
    if (expected.when) sp_expect_str_eq_c(t, spn_when_to_str(mem, &req->when), expected.when);
    if (expected.options) sp_expect_str_eq_c(t, spn_when_to_str(mem, &req->options), expected.options);
  }

  // Options
  sp_carr_for(it->options, o) {
    option_t expected = it->options[o];
    if (!expected.name) break;

    spn_option_info_t** slot = sp_str_om_getp(pkg.options, sp_str_view(expected.name));
    sp_must(t, slot);
    spn_option_info_t* option = *slot;
    sp_expect_str_eq_c(t, option->name, expected.name);
    sp_expect_eq(t, (u32)expected.type, (u32)option->type);
    sp_expect_eq(t, expected.additive, option->additive);
    sp_expect_eq(t, expected.public, option->public);
    if (expected.define) sp_expect_str_eq_c(t, option->define, expected.define);
    sp_must_strs_eq(t, option->values, sp_da_size(option->values), expected.values);

    u32 num_defaults = 0;
    sp_carr_detect_len(expected.defaults, num_defaults, expected.defaults[num_defaults].value);
    sp_must_eq(t, num_defaults, (u32)sp_da_size(option->defaults));
    sp_for(j, num_defaults) {
      sp_expect_str_eq_c(t, spn_when_to_str(mem, &option->defaults[j].when), expected.defaults[j].when ? expected.defaults[j].when : "always");
      sp_expect_str_eq_c(t, spn_option_value_to_str(mem, option->defaults[j].value), expected.defaults[j].value);
    }
  }

  // Toolchains
  sp_carr_for(it->toolchains, c) {
    toolchain_t expected = it->toolchains[c];
    if (!expected.name) break;

    spn_toolchain_info_t* tc = sp_str_om_get(pkg.toolchains, sp_str_view(expected.name));
    sp_must(t, tc);
    sp_expect_eq(t, expected.remote, tc->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION);

    if (expected.url)      sp_expect_str_eq_c(t, tc->hosts[0].artifact.url, expected.url);
    if (expected.sha256)   sp_expect_str_eq_c(t, tc->hosts[0].artifact.sha256, expected.sha256);
    if (expected.mirrors)  sp_expect_str_eq_c(t, tc->hosts[0].artifact.mirror_list, expected.mirrors);
    if (expected.compiler) sp_expect_str_eq_c(t, tc->compiler.program, expected.compiler);
    if (expected.linker)   sp_expect_str_eq_c(t, tc->linker.program, expected.linker);
    if (expected.archiver) sp_expect_str_eq_c(t, tc->archiver.program, expected.archiver);
    if (expected.cxx)      sp_expect_str_eq_c(t, tc->cxx.program, expected.cxx);
    if (expected.driver)   sp_expect_eq(t, (u32)expected.driver, (u32)tc->driver);

    sp_must_strs_eq(t, tc->compiler.args, sp_da_size(tc->compiler.args), expected.args);
    sp_must_strs_eq(t, tc->cxx.args, sp_da_size(tc->cxx.args), expected.cxx_args);

    sp_carr_for(expected.targets, r) {
      spn_triple_t triple = expected.targets[r];
      if (triple.arch == SPN_ARCH_NONE) break;
      sp_must(t, r < sp_da_size(tc->targets));
      sp_expect_eq(t, (u32)triple.arch, (u32)tc->targets[r].arch);
      sp_expect_eq(t, (u32)triple.os, (u32)tc->targets[r].os);
      sp_expect_eq(t, (u32)triple.abi, (u32)tc->targets[r].abi);
    }
  }

  // Profiles
  sp_carr_for(it->profiles, pr) {
    profile_t expected = it->profiles[pr];
    if (!expected.name) break;

    spn_profile_info_t* p = sp_str_om_get(pkg.profiles, sp_str_view(expected.name));
    sp_must(t, p);
    sp_expect_str_eq_c(t, p->name, expected.name);
    if (expected.toolchain) sp_expect_str_eq_c(t, p->toolchain, expected.toolchain);
    sp_expect_eq(t, (u32)expected.linkage, (u32)p->linkage);
    if (expected.standard) sp_expect_eq(t, (u32)expected.standard, (u32)p->standard);
    if (expected.mode) sp_expect_eq(t, (u32)expected.mode, (u32)p->mode);
    if (expected.opt) sp_expect_eq(t, (u32)expected.opt, (u32)p->opt);
    sp_expect_eq(t, expected.sanitizers, p->sanitizers);
    sp_expect_eq(t, expected.sanitizers_set, p->sanitizers_set);
    sp_expect_eq(t, (u32)expected.os, (u32)p->os);
    sp_expect_eq(t, (u32)expected.arch, (u32)p->arch);
    if (expected.abi) sp_expect_eq(t, (u32)expected.abi, (u32)p->abi);
    if (expected.options) sp_expect_str_eq_c(t, spn_when_to_str(mem, &p->options), expected.options);
  }

  // Indexes
  sp_carr_for(it->indexes, x) {
    index_t expected = it->indexes[x];
    if (!expected.name) break;

    spn_index_info_t* idx = sp_str_om_get(pkg.indexes, sp_str_view(expected.name));
    sp_must(t, idx);
    if (expected.url) sp_expect_str_eq_c(t, idx->protocol == SPN_INDEX_PROTOCOL_HTTP ? idx->http.url : idx->git.url, expected.url);
    if (expected.path) sp_expect_str_eq(t, idx->dir.path, sp_fs_join_path(mem, ctx.dir, sp_cstr_as_str(expected.path)));
    sp_expect_eq(t, (u32)expected.protocol, (u32)idx->protocol);
    sp_expect_eq(t, (u32)expected.kind, (u32)idx->kind);
  }

  // Patches
  sp_carr_for(it->patches, pt) {
    patch_t expected = it->patches[pt];
    if (!expected.name) break;

    spn_pkg_patch_t* patch = SP_NULLPTR;
    sp_da_for(pkg.patches, j) {
      if (sp_str_equal(pkg.patches[j].qualified, sp_str_view(expected.name))) {
        patch = &pkg.patches[j];
        break;
      }
    }
    sp_must(t, patch);

    u32 num_files = 0;
    sp_carr_detect_len(expected.files, num_files, expected.files[num_files]);
    sp_must_eq(t, num_files, (u32)sp_da_size(patch->set.files));
    sp_for(j, num_files) {
      sp_expect(t, sp_fs_is_absolute(patch->set.files[j]));
      sp_expect(t, sp_str_ends_with(patch->set.files[j], sp_str_view(expected.files[j])));
    }

    sp_expect_eq(t, expected.hashed, patch->set.hash != 0);
  }

  // Config
  sp_carr_for(it->config, ct) {
    config_t expected = it->config[ct];
    if (!expected.key) break;

    spn_pkg_config_entry_t* entry = SP_NULLPTR;
    sp_da_for(pkg.config, j) {
      if (sp_str_equal(pkg.config[j].key, sp_str_view(expected.key))) {
        entry = &pkg.config[j];
        break;
      }
    }
    sp_must(t, entry);
    if (expected.kind) {
      sp_must(t, !sp_opt_is_null(entry->value.kind));
      sp_expect_eq(t, (u32)expected.kind, (u32)sp_opt_get(entry->value.kind));
    }
    if (expected.options) sp_expect_str_eq_c(t, spn_when_to_str(mem, &entry->value.options), expected.options);
    sp_expect_eq(t, expected.defaults_declined, entry->value.defaults_declined);
  }

  return SP_OK;
}
