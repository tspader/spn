# Library Build Systems

## By Build System

### CMake
- libclang (libclang.mk - custom cmake)
- md4c
- uv
- freetype
- glfw
- whisper
- sdl3
- raylib
- msgpack
- ggml
- curl

### Make
#### configure + make
- **sqlite** - autoconf configure + make
- **sodium** - autogen + configure + make

#### plain make
- **quickjs**
- **treesitter**
- **luajit**

### CC directly
- **imgui** - `builder:cc()` for shared build only

### Header-only / Source-only
- **utest** (single header)
- **toml** (single header)
- **sp** (single header)
- **argparse** (single header)
- **mongoose** (source-only)
- **dr_libs** (header-only)
- **cr** (single header)
- **stb** (header-only)
- **sokol** (header-only)
- **nuklear** (header-only)
- **hmm** (single header)
- **miniz** (source-only)
- **sds** (source-only)
- **linenoise** (source-only)
- **cjson** (source-only)
- **microui** (source-only)
- **clay** (header-only)

## Debug vs Release (plain make libs)

### sqlite
- configure + make
- **debug**: `./configure --debug` → enables `-g -DSQLITE_DEBUG=1`
- **release**: default (no `--debug`)

### quickjs
- plain make
- **debug**: `make CFLAGS="$(CFLAGS_DEBUG)"` where `CFLAGS_DEBUG = -O0`
- **release**: default uses `CFLAGS_OPT = -O2`
- alt: build `qjs-debug` target for debug binary

### sodium
- autogen + configure + make
- **debug**: `./configure CFLAGS="-g -O0"`
- **release**: `./configure CFLAGS="-O2"` or default (autotools defaults to `-g -O2`)

### treesitter
- plain make
- **debug**: `make CFLAGS="-g -O0"`
- **release**: default `CFLAGS ?= -O3 -Wall...`

### luajit
- plain make
- **debug**: uncomment `CCDEBUG= -g` in src/Makefile or `make CCDEBUG=-g`
- **release**: default `CCOPT= -O2` (already optimized)

## Static vs Shared

### sqlite
- configure + make
- **static only**: `./configure --disable-shared`
- **shared only**: `./configure --disable-static`
- **both**: default

### quickjs
- plain make
- **static only**: builds `libquickjs.a` (no shared lib support)
- can build `.so` modules for examples but not main lib

### sodium
- autogen + configure + make
- **static only**: `./configure --disable-shared --enable-static`
- **shared only**: `./configure --enable-shared --disable-static`
- **both**: default (autotools standard)

### treesitter
- plain make
- **static only**: `make static` → builds `libtree-sitter.a`
- **shared only**: `make shared` → builds `libtree-sitter.so`
- **both**: `make` or `make all`

### luajit
- plain make
- **static**: `make BUILDMODE=static` → static lib + statically linked binary
- **shared**: `make BUILDMODE=dynamic` → shared lib + dynamically linked binary
- **both**: `make BUILDMODE=mixed` (default) → both libs + statically linked binary

## Smoke Test Failures

### quickjs
- **error**: `make[2]: *** No targets specified and no makefile found.  Stop.`
- **cause**: `builder:make()` missing `directory` parameter
- runs make in build dir (empty) instead of source dir where Makefile is
- **fix**: add `directory = builder.paths.source`

### sodium
- **error**: `aclocal: error: 'configure.ac' is required`
- **cause**: autogen.sh fails to find configure.ac
- autogen runs in source dir but aclocal can't find configure.ac (exists at /home/spader/.local/share/spn/cache/source/sodium/configure.ac)
- likely missing autotools dependencies or autogen needs different invocation

### treesitter
- **error**: `make[2]: *** No targets specified and no makefile found.  Stop.`
- **cause**: make not running in source dir despite recipe specifying `directory = builder.paths.source`
- Makefile exists at /home/spader/.local/share/spn/cache/source/treesitter/Makefile
- build system bug with directory parameter
