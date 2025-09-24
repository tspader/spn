# SPN Examples

SPN is a C build system/runtime with Lua-based package management. The examples demonstrate library integrations and serve as both validation tests and reference implementations.

## Project Structure

- **`source/`** - Core C runtime (`main.c`, `spn.h`) and Lua helpers
- **`asset/recipes/`** - Package recipes (Make/Lua) for dependencies  
- **`examples/`** - Self-contained demos, each with `main.c` + `spn.lua`
- **`build/`** - Output directory (disposable, auto-generated)

## Building Examples

```bash
# Build specific example
make <example_name>  # e.g., make freetype

# Build all examples  
make examples

# Run example
./build/examples/<name>/main  # e.g., ./build/examples/freetype/main
```

## Example Structure

Each example contains:
- `main.c` - C implementation demonstrating library usage
- `spn.lua` - Build configuration and dependencies

Examples validate the build system and provide copy-pasteable integration patterns for libraries like SDL, FreeType, Dear ImGui, etc.