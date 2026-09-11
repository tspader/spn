#include "env.h"
#include "render.h"

typedef struct {
  const c8* name;
  const c8* project;
  const c8* config;
  const c8* toolchain;
  const c8* path;
  const c8* copy [4];
  const c8* setup [2][SPN_TEST_COMMAND_MAX_ARGS];
  const c8* args [SPN_TEST_COMMAND_MAX_ARGS];
  const c8* env [SPN_TEST_COMMAND_MAX_ENV];
} cell_t;

static cell_t cells [] = {
  {
    .name = "help",
    .args = { "--help" },
  },
  {
    .name = "usage_unknown_flag",
    .args = { "build", "--bogus" },
  },
  {
    .name = "index_bare",
    .args = { "index" },
  },
  {
    .name = "build_cold",
    .project = "test/render/fixtures/ok",
    .args = { "build" },
  },
  {
    .name = "build_fail",
    .project = "test/render/fixtures/compile_error",
    .args = { "build" },
  },
  {
    .name = "build_json",
    .project = "test/render/fixtures/ok",
    .args = { "build", "--json" },
  },
  {
    .name = "build_warm",
    .project = "test/render/fixtures/ok",
    .setup = { { "build" } },
    .args = { "build" },
  },
  {
    .name = "build_quiet",
    .project = "test/render/fixtures/ok",
    .args = { "build", "-q" },
  },
  {
    .name = "build_verbose",
    .project = "test/render/fixtures/ok",
    .args = { "build", "-v" },
  },
  {
    .name = "build_no_manifest",
    .args = { "build" },
  },
  {
    .name = "manifest_invalid",
    .project = "test/render/fixtures/bad_manifest",
    .args = { "build" },
  },
  {
    .name = "usage_bad_choice",
    .args = { "build", "--mode", "bogus" },
  },
  {
    .name = "test_ok",
    .project = "test/render/fixtures/test_ok",
    .args = { "test" },
  },
  {
    .name = "test_fail",
    .project = "test/render/fixtures/test_error",
    .args = { "test" },
  },
  {
    .name = "index_list",
    .args = { "index", "list" },
  },
  {
    .name = "init",
    .args = { "init", "B" },
  },
  {
    .name = "version",
    .args = { "-V" },
  },
  {
    .name = "build_cold_color",
    .project = "test/render/fixtures/ok",
    .args = { "build" },
    .env = { "CLICOLOR_FORCE=1" },
  },
  {
    .name = "build_fail_color",
    .project = "test/render/fixtures/compile_error",
    .args = { "build" },
    .env = { "CLICOLOR_FORCE=1" },
  },
  {
    .name = "build_no_color",
    .project = "test/render/fixtures/ok",
    .args = { "build", "--no-color" },
    .env = { "CLICOLOR_FORCE=1" },
  },
  {
    .name = "err_toolchain_unknown",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "nope",
    .args = { "build" },
  },
  {
    .name = "err_toolchain_host",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "msvc",
    .args = { "build" },
  },
  {
    .name = "err_toolchain_none",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "--sanitize", "memory", "--abi", "musl" },
  },
  {
    .name = "err_toolchain_target",
    .project = "test/render/fixtures/errors/base",
    .config = "wasi_only.toml",
    .toolchain = "wasi-only",
    .args = { "build" },
  },
  {
    .name = "err_toolchain_sysroot",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "clang",
    .args = { "build", "--os", "windows" },
  },
  // error: toolchain llvm needs the MSVC SDK to build for x86_64-windows-msvc
  // it can build for: x86_64-linux-gnu
  // toolchains that can: zig
  //
  // Zig can't build for MSVC without the SDK either...?
  {
    .name = "err_toolchain_sdk_msvc",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "llvm",
    .args = { "build", "--os", "windows" },
  },
  {
    .name = "err_toolchain_sdk_macos",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "clang",
    .args = { "build", "--target", "aarch64-macos" },
  },
  {
    .name = "err_toolchain_missing",
    .project = "test/render/fixtures/errors/base",
    .config = "missing.toml",
    .toolchain = "no-program",
    .args = { "build" },
  },
  {
    .name = "err_toolchain_msvc_linker_host",
    .project = "test/render/fixtures/errors/msvc",
    .config = "msvc_linker.toml",
    .toolchain = "msvc-host",
    .path = "bin",
    .copy = { "bin" },
    .args = { "build", "--target", "x86_64-windows-msvc" },
  },
  {
    .name = "err_toolchain_no_cxx",
    .project = "test/render/fixtures/errors/cxx",
    .config = "no_cxx.toml",
    .toolchain = "no-cxx",
    .path = "bin",
    .copy = { "bin", "main.cpp" },
    .args = { "build" },
  },
  {
    .name = "err_toolchain_fetch",
    .project = "test/render/fixtures/errors/artifact",
    .config = "fetch.toml",
    .toolchain = "D",
    .args = { "build" },
  },
  // error: toolchain D downloaded from file://$FIXTURE/payload.tar.gz has sha256 904330787961428ffcb3dc757d9f319755fb279da86413e97d37ad356a2cf8cf; expected 0000000000000000000000000000000000000000000000000000000000000000
  //
  // Clearly needs a new line for each:
  // error: toolchain D downloaded from file://$FIXTURE/payload.tar.gz has wrong sha256:
  //   904330787961428ffcb3dc757d9f319755fb279da86413e97d37ad356a2cf8cf (actual)
  //   0000000000000000000000000000000000000000000000000000000000000000 (expected)
  //
  // Or maybe thing about how actual SHA verification tools print this
  {
    .name = "err_toolchain_sha",
    .project = "test/render/fixtures/errors/artifact",
    .config = "sha.toml",
    .toolchain = "D",
    .copy = { "payload.tar.gz" },
    .args = { "build" },
  },
  {
    .name = "err_toolchain_extract",
    .project = "test/render/fixtures/errors/artifact",
    .config = "extract.toml",
    .toolchain = "D",
    .copy = { "payload.tar.gz" },
    .args = { "build" },
  },
  {
    .name = "err_target_abi",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "--os", "windows" },
  },
  // error: A doesn't support static linkage (requested by the profile)
  //
  // @fix Name the library, too. Linkage is a property of a specific library.
  {
    .name = "err_target_linkage",
    .project = "test/render/fixtures/errors/lib",
    .args = { "build" },
  },
  // error: toolchain zig can't build for x86_64-linux-musl with address
  // it supports: thread, undefined
  //
  // I don't know if I'm a fan of the current style of listing the supported list. Like,
  // another example:
  //
  // error: toolchain llvm needs the MSVC SDK to build for x86_64-windows-msvc
  // it can build for: x86_64-linux-gnu
  // toolchains that can: zig
  //
  // Just feels a bit...loose? Prose-y? That one has other problems, I guess.
  {
    .name = "err_sanitizer_unsupported",
    .project = "test/render/fixtures/errors/base",
    .toolchain = "zig",
    .args = { "build", "--sanitize", "address" },
  },
  // error: address can't be linked statically; set linkage = "shared" in the profile
  //
  // Not "address", "ASan" or "address sanitizer"
  {
    .name = "err_sanitizer_static",
    .project = "test/render/fixtures/errors/static",
    .toolchain = "gcc",
    .args = { "build", "--sanitize", "address" },
  },
  // @spader
  //
  // error: toolchain zig can't use linker scripts for x86_64-windows-gnu
  //
  // Why is this? I forgot
  {
    .name = "err_feature_linker_script",
    .project = "test/render/fixtures/errors/script",
    .toolchain = "zig",
    .copy = { "main.ld" },
    .args = { "build", "--target", "x86_64-windows-gnu" },
  },
  // @spader
  //
  // error: toolchain zig can't link shared libraries for wasm32-wasi-musl
  //
  // Because nobody can, right? It's a property of WASM, not Zig
  {
    .name = "err_feature_link_shared",
    .project = "test/render/fixtures/errors/shared_lib",
    .toolchain = "zig",
    .args = { "build", "--target", "wasm32-wasi-musl" },
  },
  // @spader
  //
  // error: toolchain zig can't link frameworks without a macOS SDK for aarch64-macos-apple
  //
  // This is a natural, unavoidable footgun. You're cross compiling any macOS code that uses
  // a framework, which means you need the SDK, but since you're crossing you don't have
  // one. The current message isn't wrong or even really bad, it's just that we need to
  // give more context
  {
    .name = "err_feature_frameworks",
    .project = "test/render/fixtures/errors/frameworks",
    .toolchain = "zig",
    .args = { "build", "--target", "aarch64-macos" },
  },
  {
    .name = "err_profile_undefined",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "-p", "nope" },
  },
  {
    .name = "err_profile_invalid",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "-p", "a/b" },
  },
  // error: invalid target wasm32-linux; linux doesn't run on wasm32
  //
  // Kind of weird
  {
    .name = "err_profile_arch",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "--arch", "wasm32" },
  },
  {
    .name = "err_triple_invalid",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "--target", "bogus" },
  },
  // error: invalid target x86_64-linux-msvc; linux has no msvc abi
  //
  // Kind of weird. I get it, but the ABI isn't a property of the OS. It's a property
  // of the distro. Couldn't you theoretically write an MSVC ABI Linux? Or is the difference
  // between musl and GNU confusing me -- like, can you have an MSVC ABI ELF binary? Or is
  // ELF fundamentally SYSV?
  {
    .name = "err_profile_abi",
    .project = "test/render/fixtures/errors/base",
    .args = { "build", "--abi", "msvc" },
  },
  {
    .name = "err_profile_linkage",
    .project = "test/render/fixtures/errors/shared",
    .args = { "build", "--target", "wasm32-wasi-musl" },
  },
  {
    .name = "err_issue_unrooted_relative",
    .project = "test/render/fixtures/errors/base",
    .config = "unrooted_relative.toml",
    .args = { "build" },
  },
  // @spader
  //
  // error: invalid manifest (./.home/config/spn/spn.toml)
  //   - path /usr/bin/gcc at toolchain[0].compiler must be relative to the downloaded toolchain
  //   - path bin/../ar has a '.', '..', or empty component
  //
  // It's not really an invalid manifest, it's syntactically and schematically correct. It's
  // that the toolchain is invalid. Separately, shouldn't we name the toolchain?
  {
    .name = "err_issue_absolute",
    .project = "test/render/fixtures/errors/base",
    .config = "absolute.toml",
    .args = { "build" },
  },
  {
    .name = "err_issue_missing_key",
    .project = "test/render/fixtures/errors/base",
    .config = "missing_key.toml",
    .args = { "build" },
  },
  // error: invalid manifest (./.home/config/spn/spn.toml)
  //   - invalid value at toolchain[0].linker
  //
  // OK, what ARE the valid values?
  {
    .name = "err_issue_invalid_value",
    .project = "test/render/fixtures/errors/base",
    .config = "invalid_value.toml",
    .args = { "build" },
  },
  {
    .name = "err_issue_unknown_key",
    .project = "test/render/fixtures/errors/unknown_key",
    .args = { "build" },
  },
  {
    .name = "err_issue_duplicate_key",
    .project = "test/render/fixtures/errors/base",
    .config = "duplicate.toml",
    .args = { "build" },
  },
};

static sp_str_t scrub(sp_mem_t mem, fixture_t* fixture, sp_str_t text) {
  text = str_replace_all(mem, text, fixture->root, sp_str_lit("$FIXTURE"));
  return str_replace_all(mem, text, sp_str_replace_c8(mem, fixture->root, '\\', '/'), sp_str_lit("$FIXTURE"));
}

static void colored_env(const c8* const* cell, const c8** out) {
  out[0] = "CLICOLOR_FORCE=1";
  u32 written = 1;
  sp_for(it, SPN_TEST_COMMAND_MAX_ENV - 1) {
    if (!cell[it]) {
      break;
    }
    out[written++] = cell[it];
  }
  out[written] = SP_NULLPTR;
}

// Every cell runs with color forced so view can show it; the plain file keeps the
// color only when the cell itself asked for it.
static bool forces_color(const c8* const* env) {
  sp_for(it, SPN_TEST_COMMAND_MAX_ENV) {
    if (!env[it]) {
      return false;
    }
    if (sp_str_starts_with(sp_cstr_as_str(env[it]), sp_str_lit("CLICOLOR_FORCE="))) {
      return true;
    }
  }
  return false;
}

static sp_str_t strip_ansi(sp_mem_t mem, sp_str_t text) {
  sp_io_dyn_mem_writer_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &buf);
  u32 it = 0;
  while (it < text.len) {
    if (text.data[it] == '\x1b' && it + 1 < text.len && text.data[it + 1] == '[') {
      it += 2;
      while (it < text.len && ((u8)text.data[it] < 0x40 || (u8)text.data[it] > 0x7e)) {
        it++;
      }
      it++;
      continue;
    }
    sp_io_write_c8(&buf.base, text.data[it++]);
  }
  return sp_io_dyn_mem_writer_take_str(&buf);
}

static void write_capture(sp_mem_t mem, sp_str_t dir, const c8* stream, sp_str_t text, bool color) {
  write_file(sp_fs_join_path(mem, dir, sp_fmt(mem, "{}.color", sp_fmt_cstr(stream)).value), text);
  write_file(sp_fs_join_path(mem, dir, sp_cstr_as_str(stream)), color ? text : strip_ansi(mem, text));
}

static void expand_fixture_token(fixture_t* fixture, sp_str_t path) {
  sp_mem_t mem = fixture->mem;
  sp_str_t content = test_read_file(mem, path);
  if (!sp_str_contains(content, sp_str_lit("@fixture@"))) {
    return;
  }
  sp_str_t root = sp_str_replace_c8(mem, fixture->root, '\\', '/');
  write_file(path, str_replace_all(mem, content, sp_str_lit("@fixture@"), root));
}

sp_test_each(render, cells, cell_t, cells) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  fixture.toolchain = it->toolchain;
  fixture.path = it->path;
  sp_try(prepare_test(t, &fixture, it->project, it->copy));

  if (it->config) {
    sp_try(fixture_config_append(t, &fixture, it->project, it->config));
    expand_fixture_token(&fixture, sp_fs_join_path(fixture.mem, fixture.paths.config, sp_str_lit("spn/spn.toml")));
  }

  sp_carr_for(it->setup, step) {
    if (!it->setup[step][0]) {
      break;
    }
    run_spn(t, &fixture, it->setup[step], SP_NULLPTR);
  }

  const c8* env [SPN_TEST_COMMAND_MAX_ENV + 1] = sp_zero;
  colored_env(it->env, env);
  sp_ps_output_t output = run_spn(t, &fixture, it->args, env);

  sp_mem_t mem = fixture.mem;
  sp_str_t dir = sp_fs_join_path(mem, render_out_path(mem, "current"), sp_str_view(it->name));
  bool color = forces_color(it->env);
  write_capture(mem, dir, "stderr", scrub(mem, &fixture, output.err), color);
  write_capture(mem, dir, "stdout", scrub(mem, &fixture, output.out), color);
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("exit")), sp_fmt(mem, "{}\n", sp_fmt_int(output.status.exit_code)).value);
  return SP_OK;
}
