# overview
Ensure that include directories added via the build script at both the package level and target level work.

# test
- Run `spn build --profile debug`
- Verify that the binary `./build/debug/store/bin/main` exists, runs, and prints the correct values for `package()` and `target()`
