# Repository Guidelines

## Project Structure & Module Organization
- `source/` holds the core C entrypoint (`main.c`, `spn.h`) and Lua helpers powering the runtime.
- `asset/recipes/` contains package recipes in Make or Lua; treat them as the canonical examples for new dependencies.
- `examples/` bundles self-contained sample projects; keep new demos minimal and include a matching `spn.lua`.
- `build/` is disposable output (`build/bin/spn`, cached deps); never vendor artifacts in commits.

## Build, Test, and Development Commands
- `make bootstrap` only needed on a fresh clone; builds SDL, LuaJIT, and seeds `build/bin/spn`.
- `make build` recompiles the CLI with the existing bootstrapper; run before pushing to ensure a clean binary.
- `make install` installs `spn` to `${HOME}/.local/bin`; use `make uninstall` to remove it.
- `make examples` validates recipes by compiling each sample into `build/examples/<name>`.
- `spn init`, `spn add <pkg>`, `spn build`, and `spn print --compiler gcc` exercise the CLI; quote them when documenting behaviour.

## Coding Style & Naming Conventions
- C sources use two-space indentation, snake_case for functions, and uppercase macros; prefer `spn_`/`sp_` prefixes for new APIs.
- Lua modules mirror that casing and start with `local module = {}`; keep tables immutable unless mutation is explicit.
- Keep includes grouped by domain (platform, libs, project) and favour early returns for error paths.

## Testing Guidelines
- No formal unit suite yet; rely on `make build` plus targeted `make examples` runs that cover the recipes you touched.
- For Lua changes, add a lightweight example or extend an existing one to demonstrate behaviour.

## Commit & Pull Request Guidelines
- NEVER RUN GIT. NEVER COMMIT ANYTHING, EVER. NEVER ADD. NEVER, EVER RUN GIT.
