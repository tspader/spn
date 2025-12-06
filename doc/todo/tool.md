- `spn tool install foo` -> try to find package for `foo` in registries (do not try to load an spn.toml)
- `spn tool install .` -> look in `.` for `spn.toml` and use that as the package
- `spn tool run $package` run the first binary provided by a package
- `spn tool upgrade $package` equivalent of `spn upgrade` on internal spn.toml

- "install a tool" means to build a package and add any `[[bin]]` entries to ~/.local/bin
- we'll keep an `spn.toml` in ~/.local/share/spn/tools
- we'll build just as if we were running `spn build` builds all the tools into the spn cache, just like normal (user doesn't run this manually, just as a point of reference)
- we need to copy the binary + any shared libraries into the right place; for now, we can ignore shared libraries. we'll use `spn link`
