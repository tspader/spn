# lanes

A lane is a toolchain the whole integration suite runs under. `SPN_TEST_TOOLCHAIN`
names it; unset means `zig`. A lane is either a builtin from
`source/core/toolchain/toolchains.json` (`zig`, `gcc`, `clang`, `llvm`, `msvc`)
or a test-only toolchain from `test/tools/toolchains.json` (`gcc-lld`,
`aarch64-gnu`, `mingw-gnu`, `clang-mingw`, `clang-msvc`). Both files use the
builtin catalog schema. The harness renders the test-only entries into every
fixture's user config as `[[toolchain]]` blocks, so spn sees a lane toolchain
exactly the way `~/.config/spn/spn.toml` would declare it.

```
SPN_TEST_TOOLCHAIN=gcc-lld build/debug/test/integration   # natively
tools/docker/build/debug/smoke test gcc-lld               # in the container that hosts it
tools/docker/build/debug/smoke test debian-llvm           # every lane that image hosts
tools/docker/build/debug/smoke test                       # every lane once
tools/docker/build/debug/smoke list                       # images and the lanes they host
```

Build the container tool once with `spn build` in `tools/docker`. It mounts this
checkout, its git dir, and the toolchain caches into the image and runs
`build/debug/test/integration` there as your user, so run `spn build` and
`spn build --test integration` first.

A lane must work. If its toolchain's programs aren't on PATH, or it doesn't
support this host, the binary exits before running anything. A lane whose
`link_args` name a linker lists that binary per linker dialect in
`test/tools/caps.c`, and the check covers the dialects the lane's targets reach
on this host. Skips are for cases, never for lanes.

## gating

`.when` on a case is a set of predicates. A case runs only if every one holds
and skips with a reason otherwise.

- `.lanes = { "gcc-lld", ... }`: only in one of these lanes. Naming a lane that
  doesn't exist aborts the run.
- `.target`: the lane's declared `target` list must contain it. A lane claims
  only what its container proves: builtins claim the host (and, for `gcc` on
  Linux, the host's bare metal triple), and a test-only cross lane lists its
  target explicitly.
- `.linker = SPN_LD_FAMILY_LLD`: only where the lane's linker family for the
  target is that one. Cases about what a family accepts gate on this, never on
  a lane or driver name.
- `.cxx`: only where the lane declares a C++ compiler. `musl-gcc`, `clang-musl`
  and `clang-bare` have none, since Debian ships no C++ runtime for those
  targets.
- `.sanitize` also skips where a system gcc or clang targets a libc other than
  the host's, since its sanitizer runtimes are built for the host libc.
- `.driver`, `.os`, `.host`, `.shell`, and the rest are unchanged. `.programs`
  is for cases that need a program by name that the lane doesn't provide, like
  a fixture-local toolchain's `compiler = "gcc"`, never as a stand-in for a
  lane.

A case whose fixture declares its own `[[toolchain]]` names it with
`.toolchain = "F"` on the case; that is what `--toolchain` receives instead of
the lane's. It is for cases *about* declaring toolchains, like raw `link_args`
reaching a fake linker or a custom `cxx`. A case that needs a real toolchain
family belongs to a lane.

A case that needs user config of its own puts it in a file in its fixture dir
and names it with `.config = "config.toml"`. It is appended to the generated
config, so the lane catalog survives.

## sysroots

A lane target that names a `sysroot` points at a tree its container assembles,
declared as data in `tools/docker/source/variant/variant.c`: `debian-musl`
links Debian's musl headers and libs under `/sysroot/musl`, `debian-sysroot`
unpacks the arm64 libc and libgcc debs under `/sysroot/arm64`, and
`debian-wasi` names `/usr` because Debian's wasi-libc is already laid out as
one. `musl-gcc` and `clang-cross` list a target with no sysroot and lean on
the wrapper or on clang's gcc-cross discovery instead.
clang hands an `x86_64-none-elf` link to `gcc`, so `clang-bare` lives where
both are installed. Debian names its mingw libgcc directory `12-win32`, which clang 14 does not
parse as a version, so both clang mingw lanes name it with `-L`.

## what a lane proves

A lane declares its driver, `linker = "lld"` when its linker is lld, and the
raw `link_args` that make that true; anything else links with the native
linker of each target's dialect. spn renders nothing to choose a linker; it
emits `-T` only where the resolved family honors it.
The `linker.*` cases check that per family: a script sets the entry point for
gnu and lld on elf (`script_sets_entry` in the lanes that claim bare metal,
`cross_script_sets_entry` in `aarch64-gnu`) and for gnu on mingw
(`mingw_gcc_honors_script`); it is refused up front for lld on mingw, macho,
wasm, and msvc. `toolchain_link_args_reach_the_driver` witnesses
a raw arg selecting a fake linker, and the `link_flags_gated_on_linker_*` pair
proves the `linker` fact matches the family the lane declares.

| lane          | driver  | linker family              | link_args                          | hosted by                                     |
|---------------|---------|----------------------------|------------------------------------|-----------------------------------------------|
| `zig`         | zig     | lld (fixed)                |                                    | any host, `debian-zig`, `alpine-zig`          |
| `gcc`         | gcc     | native                     |                                    | `debian-gcc`, `debian-llvm`, `alpine`, ...    |
| `gcc-lld`     | gcc     | lld                        | `-fuse-ld=lld`                     | `debian-gcc`, `debian-llvm`                   |
| `clang`       | clang   | native                     |                                    | `debian-clang`, `debian-llvm`, `alpine-clang`, macOS |
| `llvm`        | clang   | lld                        | `-fuse-ld=lld`                     | `debian-llvm`, macOS with Homebrew llvm       |
| `msvc`        | msvc    | msvc (fixed)               |                                    | Windows dev shell                             |
| `clang-msvc`  | clang   | lld                        | `-fuse-ld=lld`                     | Windows with LLVM                             |
| `aarch64-gnu` | gcc     | gnu                        |                                    | `debian-cross`                                |
| `mingw-gnu`   | gcc     | gnu                        |                                    | `debian-mingw`                                |
| `clang-mingw` | clang   | native (gnu)               | `--ld-path=x86_64-w64-mingw32-ld -L<libgcc>` | `debian-mingw-clang`                |
| `clang-mingw-lld` | clang | lld                     | `-fuse-ld=lld -L<libgcc>`          | `debian-mingw-clang`                          |
| `clang-bare`  | clang   | native (gnu, via gcc)      |                                    | `debian-llvm`                                 |
| `musl-gcc`    | gcc     | gnu                        |                                    | `debian-musl`                                 |
| `clang-musl`  | clang   | gnu                        | `-rtlib=compiler-rt -unwindlib=none` | `debian-musl`, host gnu and sysroot musl    |
| `clang-wasi`  | clang   | lld (wasm)                 |                                    | `debian-wasi`                                 |
| `clang-sysroot` | clang | lld                        | `-fuse-ld=lld`                     | `debian-sysroot`                              |
| `clang-cross` | clang   | gnu                        |                                    | `debian-cross`                                |

The cross lanes (`aarch64-gnu`, `mingw-gnu`, `clang-mingw`, `clang-mingw-lld`,
`clang-wasi`, `clang-sysroot`, `clang-cross`) can't target the host, so only
cases with a matching `.target` run there; `target.cross_exe` builds for a
lane's first cross target so every one of them links something. The `clang` and `llvm`
lanes claim only the host: a bare-name clang reaches bare metal only through
whatever runtime and linker happen to be installed, so nothing is proven about
it there. On a Mac, put Homebrew's `lld` and keg-only `llvm` on PATH after the
system dirs so `clang` stays Apple's for the `clang` lane and `ld64.lld` is
found for `llvm`.
