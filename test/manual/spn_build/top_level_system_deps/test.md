- [ ] Verify that the directory is clean (just `main.c`, `spn.toml`, and `test.md`)
- [ ] Run `spn build`
  - [ ] Verify that `spn.lock` was created
  - [ ] Verify that the build succeeded and runs
  - [ ] Verify that the compiler command line in the build log (`build/work/main.build.log`) contains the system dependency exactly once
- [ ] Run `spn build --force`
  - [ ] Verify that the build succeeded and runs

Currently, the dependency resolution in the lock file code path does not take into account system dependencies used in the top level package. This is a bug.
