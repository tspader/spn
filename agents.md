# Project Overview
- **spn**: Package manager and build tool for C projects in the style of UV or cargo (spn.toml + spn.c)
- written in modern C using `sp.h` (use the `sp` skill or `./doc/sp/SKILL.md` if you do not have access to global skills)
  - `spn` is an extension of `sp.h`, so its instructions and style are exactly the same as this project's
  - reference the `sp` skill or `./doc/sp/SKILL.md` judiciously
```xml
<example>
user: Write a function that reads a file and logs its contents
assistant: [Uses Task tool and sp skill to find relevant APIs; looks through spn.c for existing contextual examples]
</example>
```

# Files
- `source/spn.c` is the entire implementation
- `include/spn/spn.h` for public API used in packages
- `spn.toml` is the package for spn itself; (downstream example)
- `packages/sp/spn.toml` is the package for `sp.h` (source-only example)
- `packages/tcc/spn.toml` is the package for `tcc` (compiled example)

# Build
- build with `make` to output to `./bootstrap/bin/spn`

## Rules
- always use the `sp.h` skill when (either with your `Skill` tool or with `./doc/llm/sp/SKILL.md`)
