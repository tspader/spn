---
title: Packages
---

Packages are defined by TOML manifests.

## Targets

A package can define as many executables, libraries, tests, and examples as it would like. These are called *targets*. They have fields like `source`, or `include`, or `system_deps`, and most fields can be configured at the package level for all targets or for an individual target.

```toml
[package]
include = ['common']

[[lib]]
source = ['source/lib.c']
include = ['source/lib']

[[example]]
source = ['example/foo/main.c']

[[test]]
source = ['test/main.c', 'test/foo.c']
define = ['SOMETHING']
system_deps = ['m']
```

A `[[bin]]` and a `[[script]]`, for instance, are both executables, but their difference isn't strictly cosmetic. `[[bin]]` entries are taken to be *exports* of your package, able to be pulled in by consumers. `[[script]]` entries aren't. `[[test]]` entries are executed in `spn test`, and compiled to `test/` instead of `bin/`.

## Paths

Paths may not contain relative components like `.` and `..`, with the exception of the literal `"."`.

## Target fields

### source

### include and headers

### define and flags

### system_deps

### deps

### kinds

### cxx

## System dependencies



## Conditional configuration

Any field in your package can be keyed on any fact of the build (target OS, architecture, ABI, build mode, optimization level, sanitizer settings). Clauses are structured data, not a DSL, and an entry with multiple clauses gets them ANDed together. Here's some common examples:

```toml
source = [
  { path = "source/backend/win32.c", when = { os = "windows" } },
  { path = "source/backend/posix.c", when = { os = { not = "windows" } } },
]
flags = [
  { value = "-mfpu=neon", when = { os = "linux", arch = "aarch64" } },
]
define = [
  { value = "USE_DEBUG_ALLOC", when = { mode = "debug", sanitize_address = false } },
]
system_deps = [
  { value = "pthread", when = { os = { not = "windows" } } },
]
deps = [
  { pkg = "tracy", when = { mode = "debug" } },
]
```

### when clauses

### Fact keys

### Options as keys

Options declared in the manifest work as keys, too, so a feature flag can gate sources and dependencies together:

```toml
[options.freetype]
type = "bool"
default = true
define = "UI_FREETYPE"

[[lib]]
name = "ui"
source = [
  { path = "source/text_freetype.c", when = { freetype = true } },
]
deps = [
  { pkg = "freetype", when = { freetype = true } },
]
```

## Options

Packages provide options. Options can be enumerations or booleans; enums are mutually exclusive, and an unresolvable conflict is a build error. Booleans are additive. For example:

```toml
[options.tls]
type = "enum"
values = ["schannel", "openssl", "off"]
default = [
  { when = { os = "windows" }, value = "schannel" },
  { when = { os = { not = "wasi" } }, value = "openssl" },
  { value = "off" },
]

[options.zstd]
type = "bool"
default = false
```

Consumers can then [set these options](/docs/dependencies#options)

## Platforms

### macOS

```toml
[package.macos]
min_os = { major = 12 }

[lib.macos]
frameworks = ["Cocoa", "IOKit", "CoreVideo", "OpenGL"]
```

When linking, your binary's `min_os` is the max across everything in it. If a dependency needs macOS 12, your binary targets macOS 12.

### Windows

```toml
[[bin]]
# ...
windows = { subsystem = "windows" }
```

## Embedding files

`spn` can embed arbitrary files and bytes (from build scripts) in your binary by creating an object file and header that anything can link to trivially. For example, this configuration:

```toml
[[bin]]
name = "whatever"
source = ["main.c"]
embed = [
  "asset/fonts/inconsolata.ttf",
  { path = "asset/data.json", symbol = "data_json", data_type = "u8", size_type = "u64" },
  { path = "asset/shaders", dir = true, dest = "shaders" },
]
```

Produces this header:

```c
extern const unsigned char asset_fonts_inconsolata_ttf [109384];
extern const unsigned long long asset_fonts_inconsolata_ttf_size;

extern const u8 data_json [2213];
extern const u64 data_json_size;

extern const unsigned char shaders_quad_frag [412];
extern const unsigned long long shaders_quad_frag_size;

extern const unsigned char shaders_quad_vert [287];
extern const unsigned long long shaders_quad_vert_size;

typedef struct {
  const char* path;
  const void* data;
  unsigned long long size;
} spn_embed_entry_t;
static const unsigned int spn_embed_count = 4;
static const spn_embed_entry_t spn_embed_manifest[] = {
  { "asset/fonts/inconsolata.ttf", asset_fonts_inconsolata_ttf, 109384 },
  { "asset/data.json", data_json, 2213 },
  { "shaders/quad.frag", shaders_quad_frag, 412 },
  { "shaders/quad.vert", shaders_quad_vert, 287 },
};
```
