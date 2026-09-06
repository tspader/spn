# lanes

A lane is a toolchain the whole integration suite runs under. `SPN_TEST_TOOLCHAIN`
names it; unset means `zig`. A lane is either a builtin from
`source/core/toolchain/toolchains.json` (`zig`, `gcc`, `clang`, `llvm`, `msvc`,
`apple-clang`) or a test-only toolchain from `test/tools/toolchains.json`
(`gcc-lld`, `aarch64-gnu`, `mingw-gnu`, `clang-mingw`, `clang-msvc`). Both files use
the builtin catalog schema. The harness renders the test-only entries into every
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
support this host, the binary exits before running anything. Skips are for
cases, never for lanes.

## gating

`.when` on a case is a set of predicates. A case runs only if every one holds
and skips with a reason otherwise.

- `.lanes = { "gcc-lld", ... }`: only in one of these lanes. Naming a lane that
  doesn't exist aborts the run.
- `.driver`, `.target`, `.os`, `.host`, `.shell`, and the rest are unchanged.
  `.programs` is for cases that need any compiler by a generic name (`cc`,
  `c++`), never as a stand-in for a lane.

A case whose fixture declares its own `[[toolchain]]` names it with
`.toolchain = "F"` on the case; that is what `--toolchain` receives instead of
the lane's. It is for cases *about* declaring toolchains, like a fake linker
program or a custom `cxx`. A case that needs a real toolchain family belongs to
a lane.

A case that needs user config of its own puts it in a file in its fixture dir
and names it with `.config = "config.toml"`. It is appended to the generated
config, so the lane catalog survives.

## what a lane proves

Every hosted build in a lane links through that lane's driver and linker
family, so the suite itself is the proof that spn hands each driver its linker
correctly. The `linker.*` cases add the linker script capability per family:
honored for gnu and lld on elf (`script_sets_entry`, in every elf lane) and gnu
on mingw (`mingw_gcc_honors_script`), refused up front for lld on mingw, macho,
wasm, and msvc.

| lane          | driver → family                          | flavors              | hosted by                                  |
|---------------|------------------------------------------|----------------------|--------------------------------------------|
| `zig`         | zig → lld (fixed)                        | elf mingw macho wasm | any host, `debian-zig`, `alpine-zig`       |
| `gcc`         | gcc → gnu                                | elf (macho on a Mac) | `debian-gcc`, `debian-llvm`, `alpine`, ... |
| `gcc-lld`     | gcc → lld (`-fuse-ld=lld`)               | elf                  | `debian-gcc`, `debian-llvm`                |
| `clang`       | clang → gnu (`--ld-path=ld`)             | elf                  | `debian-clang`, `debian-llvm`, `alpine-clang` |
| `llvm`        | clang → lld (`--ld-path=ld.lld`, `ld64.lld`) | elf macho        | `debian-llvm`, macOS with Homebrew llvm    |
| `apple-clang` | clang → ld64 (`--ld-path=ld`)            | macho                | macOS                                      |
| `msvc`        | msvc → link.exe (fixed)                  | msvc                 | Windows dev shell; CI runs it              |
| `clang-msvc`  | clang → lld-link (`-fuse-ld=lld`)        | msvc                 | Windows with LLVM                          |
| `aarch64-gnu` | cross gcc → gnu                          | elf (freestanding)   | `debian-cross`                             |
| `mingw-gnu`   | mingw gcc → gnu                          | mingw                | `debian-mingw`                             |
| `clang-mingw` | clang → gnu (`--ld-path=x86_64-w64-mingw32-ld`) | mingw         | `debian-mingw-clang`                       |

The cross lanes (`aarch64-gnu`, `mingw-gnu`, `clang-mingw`) can't target the
host, so only cases with a matching `.target` run there. On a Mac, put Homebrew's
`lld` and keg-only `llvm` on PATH after the system dirs so `clang` stays Apple's
for the `apple-clang` lane and `ld64.lld` is found for `llvm`.
