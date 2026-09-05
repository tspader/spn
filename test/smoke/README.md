# smoke: the linker & toolchain matrix

These are manual, exhaustive checks for the linker/toolchain machinery. They are
not part of `spn test`. Each case declares a toolchain, builds a tiny fixture for
one output flavor, and asserts the real link outcome. A case needs a real
(sometimes esoteric) toolchain, so it *runs* only where that toolchain exists and
*skips* everywhere else. What makes them trustworthy is that we enumerated every
cell we care about and ran each one green at least once, across containers and
real hosts.

```
sh test/smoke/run.sh            # native lane + every docker lane, then a coverage matrix
sh test/smoke/run.sh docker     # docker lanes only
sh test/smoke/run.sh <variant>  # one container by name (e.g. debian-cross)
sh test/smoke/run.sh hosts      # POSIX hosts from hosts.txt (macOS)
sh test/smoke/run.sh all        # everything reachable
spn build --test smoke && build/debug/test/smoke --filter 'msvc.*'   # by hand
```

Lanes are data: `docker.txt` (one container per toolchain family) and `hosts.txt`
(native macOS/Windows boxes, see `../../.llm/prompt/machines.md`). The runner
folds all lanes into a coverage matrix: a cell is **PROVEN** if some lane linked
it, **FAIL** if some lane failed it, **PENDING** if every lane only skipped it (no
toolchain here reached it).

## Matrix 1 — linker selection: which families a (driver, flavor) accepts, and how spn hands the driver its linker

| driver | elf          | mingw        | msvc          | macho         | wasm  |
|--------|--------------|--------------|---------------|---------------|-------|
| gcc    | gnu, lld     | gnu, lld     | —             | ld64          | —     |
| clang  | gnu, lld     | gnu, lld     | msvc, lld     | ld64, lld     | lld   |
| zig    | lld (fixed)  | lld (fixed)  | —             | lld (fixed)   | lld   |
| msvc   | —            | —            | msvc (fixed)  | —             | —     |

How the driver reaches the linker (`spn_ld_arg`):

| driver | elf / mingw / macho              | msvc                    | wasm |
|--------|----------------------------------|-------------------------|------|
| gcc    | family only; `lld` → `-fuse-ld=lld` | —                    | —    |
| clang  | `--ld-path=<program>` (program required) | `-fuse-ld=lld` iff lld | none |
| zig    | none (fixed)                     | —                       | none |
| msvc   | —                                | none (fixed)            | —    |

Fixed drivers (`zig`, `msvc`) take no `linker` key; declaring one is a manifest
error. One exception lives in the arg table: clang linking x86_64 freestanding
goes *through gcc*, so that cell uses gcc's rules.

## Matrix 2 — capability: does a (family, flavor) honor a linker script?

| family | elf | mingw | msvc | macho | wasm |
|--------|-----|-------|------|-------|------|
| gnu    | yes | yes\* | —    | —     | —    |
| lld    | yes | no    | no   | no    | no   |
| ld64   | —   | —     | —    | no    | —    |
| msvc   | —   | —     | no   | —     | —    |

\* gnu@mingw also carries `--exclude-libs`, the only place spn uses it. Every
"no"/absent cell means spn refuses a `linker_script` up front with
`compiler_feature_unsupported` rather than handing the linker a flag it ignores.

## Matrix 3 — the smoke cells: one real link per interesting (flavor, driver, family)

`B` = builds and links (asserts `link_passed`); `E` = script sets the ELF entry
to `0x400000`; `R` = script correctly refused. "Proven in" is where it has been
run green.

| cell | driver → family | asserts | proven in |
|------|-----------------|---------|-----------|
| `elf.gnu_script`        | gcc → gnu           | E | debian-llvm, aral |
| `elf.lld_script`        | gcc → lld (`-fuse-ld=lld`) | E | debian-llvm, aral |
| `elf.clang_gnu`         | clang → gnu (`--ld-path=ld`) | B | debian-llvm, debian-mingw-clang, aral |
| `elf.clang_lld`         | clang → lld (`--ld-path=ld.lld`) | B | debian-llvm, aral |
| `elf.gcc_lld_hosted`    | gcc → lld (swap default linker) | B | debian-llvm, aral |
| `elf.zig_script`        | zig → lld           | E | debian-zig, aral, piotr |
| `elf.cross_gnu_script`  | aarch64 cross gcc → gnu | E | debian-cross |
| `mingw.gnu_script`      | gcc → gnu (mingw)   | B | debian-mingw-clang, aral, piotr |
| `mingw.zig_build`       | zig → lld           | B | debian-zig, aral, piotr |
| `mingw.zig_script_rejected` | zig → lld       | R | debian-zig, aral, piotr |
| `mingw.clang_gnu`       | clang → gnu (`--ld-path`, needs mingw sysroot) | B | debian-mingw-clang, aral |
| `msvc.link`             | msvc → msvc (link.exe) | B | piotr |
| `msvc.clang_lld`        | clang → lld (`-fuse-ld=lld`) | B | piotr |
| `msvc.script_rejected`  | msvc → msvc         | R | piotr |
| `macho.ld64_apple`      | clang → ld64 (`--ld-path=ld`) | B | *pending (miles)* |
| `macho.ld64_lld`        | clang → lld (`--ld-path=ld64.lld`) | B | *pending (miles)* |
| `macho.zig_build`       | zig → lld           | B | *pending (miles)* |
| `macho.script_rejected` | zig → lld           | R | *pending (miles)* |
| `wasm.zig_build`        | zig → lld           | B | debian-zig, aral, piotr |
| `wasm.zig_script_rejected` | zig → lld        | R | debian-zig, aral, piotr |

### Notes on the host-only cells

- **macOS (`macho.*`)**: no container can produce a real ld64 link, so these need
  a Mac (`miles` in machines.md). They were pending at last run because miles was
  offline. Bring it up and `sh test/smoke/run.sh hosts` fills them in.
- **Windows (`msvc.*`)**: `cl`/`link.exe` need a Visual Studio environment, so
  run these by hand in a dev shell:
  ```
  spn build -p mingw --test smoke              # cross-build smoke.exe on Linux
  scp the tree + build/x86_64-windows-gnu/mingw/test/smoke.exe to piotr
  pwsh: tools/devenv.ps1; ./…/smoke.exe --filter '*'
  ```
  `msvc.link`, `msvc.clang_lld`, and `msvc.script_rejected` pass this way.
- **`mingw.clang_gnu`** needs a clang that ships a mingw sysroot (the
  `debian-mingw-clang` container, or msys2 `clang64`). A bare LLVM clang with no
  mingw runtime fails at link with `unable to find library -lgcc_eh`; that is the
  toolchain lacking a runtime, not spn.

## Adding a cell

Add a fixture under `fixtures/<name>/` (a `spn.toml` that declares the toolchain,
plus `main.c`, plus `main.ld` for a script case) and a `sp_test` row in the
matching `<flavor>.c`. Gate it with `.when` on `host`, `driver`, and the
`programs` it needs so it skips cleanly where the toolchain is absent — a fresh
checkout must never see a smoke failure, only skips.
