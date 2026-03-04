# cache verification checklist

verify after: `rm -rf ~/.local/share/spn && bspn build --profile debug`

## index

- [ ] `~/.local/share/spn/index/<name>-<16hex>/` exists for each git index
- [ ] working tree is up to date (`git status` shows no divergence from remote)
- [ ] `core/<pkg>.jsonl` exists for each published package
- [ ] each JSONL line has `source.url`, `source.rev`
- [ ] split-manifest packages have `manifest.url`, `manifest.rev`, `manifest.dir`
- [ ] filesystem indexes are NOT cloned (location == url, no entry in index dir)

## git db

- [ ] `~/.local/share/spn/cache/source/db/<reponame>-<16hex>/` exists per unique source URL
- [ ] separate db exists per unique manifest URL (when manifest != source)
- [ ] each db is a bare repo (`HEAD`, `objects/`, `refs/` present)
- [ ] no duplicate dbs for the same URL (even if multiple packages use it)

## checkouts

- [ ] `~/.local/share/spn/cache/source/checkouts/<reponame>-<16hex>/` exists per (url, rev, dir) triple
- [ ] source checkout contains the upstream files (e.g. `toml.h`, not `spn.toml`)
- [ ] manifest checkout contains `spn.toml` + `spn.c` at the expected subdir
- [ ] same (url, rev, dir) reuses the same checkout (no duplicates)
- [ ] different revs of same url produce different checkouts
- [ ] different dirs of same url+rev produce different checkouts

## store

- [ ] `~/.local/share/spn/cache/store/<pkgname>/<buildhash>/` exists per built package
- [ ] `include/` contains installed headers (e.g. `toml.h`)
- [ ] `lib/` contains built libraries for compiled packages (e.g. `libtcc.a`)
- [ ] `bin/` contains built binaries for packages that produce them

## build

- [ ] `~/.local/share/spn/cache/build/<pkgname>/<buildhash>/` exists per built package
- [ ] contains build working files (stamps, intermediate objects)

## runtime

- [ ] `~/.local/share/spn/runtime/` exists with TCC runtime files
- [ ] `~/.local/share/spn/runtime/include/` exists

## incremental

verify after: publish a new package to the index, then build a consumer WITHOUT cleaning the cache

- [ ] `spn_index_sync` pulls new commits (index working tree has new JSONL files)
- [ ] `spn_git_db_ensure_rev` fetches when a rev is missing from an existing bare db
- [ ] existing checkouts/stores are not re-created
- [ ] new checkouts appear for newly resolved packages
