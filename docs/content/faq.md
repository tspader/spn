---
title: FAQ
---

## What about `compile_commands.json`?

`spn` emits it on every build, at your project's top level. If you were to run:

```bash
spn build
spn build --target=x86_64-windows-gnu
```

Then your `compile_commands.json` would point to MinGW stuff.

## Does it work with MSVC / Xcode?
