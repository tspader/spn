# Repository Guidelines

## Project Structure & Module Organization
- `source` holds the core C entrypoint (`main.c`, `spn.h`) and Lua helpers powering the runtime.
- `asset/recipes` contains all of the package recipes
- `examples` bundles self-contained sample projects; every dependency is one C/++ file and `spn.lua`
- `build` is disposable output (`build/bin/spn`, cached deps); never vendor artifacts in commits.

## Build, Test, and Development Commands
- `make bootstrap` only needed on a fresh clone or if your current `spn` binary is broken; builds SDL, LuaJIT, and seeds `build/bin/spn`.
- `make` builds `spn` using `spn`
- `make install` installs `spn` to `${HOME}/.local/bin`; use `make uninstall` to remove it.
- `make $example` builds a single example and puts the binary in `build/examples/$example/main`
- `make examples` validates recipes by compiling each sample into `build/examples/$example/main`
- `spn --no-interactive build`, and `spn print --compiler gcc` exercise the CLI; quote them when documenting behaviour.

## Coding Style & Naming Conventions
- C sources use two-space indentation, snake_case for functions, and uppercase macros; prefer `spn_`/`sp_` prefixes for new APIs.

## In Which Thomas Prostrates Himself to Produce a More Perfect Machine
- NEVER ADD OR USE THE C STANDARD LIBRARY; ALWAYS USE SP.H
- NEVER COMMENT
- NEVER RUN GIT. NEVER COMMIT ANYTHING, EVER. NEVER ADD. NEVER, EVER RUN GIT.
- NEVER COMMENT
