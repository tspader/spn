# overview
The missing build tool and package manager for C. Stop doing insane things like depending on *Python* to build your C code, or using an arcane DSL to define a simple DAG. Instead:
- Write TOML manifests
- Write build scripts in C, the language you're already using, and let `spn` JIT compile them on the fly instead of depending on an undebuggable, dependency-ridden scripting language
- Build from source pinned to *exact commits*; Git is not a suitable database for an index, but it sure as hell works for source code
- Search for dependencies using an index, self hostable as a Git repository or an HTTP API
- Host your own index -- all you need to do so is a Git repository! `spn` will write the metadata to it as you publish private packages
- Use `zig cc` to trivially cross compile binaries for any target
- Bask in modern quality-of-life features:
    - Your build can have dependencies!
    - Your tests can, too!
    - `spn run main.c` for zero-build deployments
    - Install native binaries, like `bun install -g` or `cargo install`
    - ...and so much more!

# state of the repo
Extremely WIP. I pushed up fifty-something WIP commits from a giant rewrite branch to show the current state. It does not, therefore, work (beyond printing out a very pretty help message). That being said, it's no vaporware. I'd been using it for three or four months as my daily driver before I decided to commit to rewriting quite a bit of code based on what I'd learned.

I'm also trying a new project structure, if it looks weird. Every module (rough and conceptual, not real modules) define:
- `$module/types.h`, for types only
- Any number of `.h` + `.c` pairs. The headers only declare functions and types specific to the TU.

I did this because I can't find a better way to actually unit test stuff otherwise. If you mix types and functions, you end up pulling in tons of headers just for the symbols. You lose the natural dependency graph that ought to come with one TU `#include`ing another's header. By doing it like this, if `foo.c` does `#include "bar.h"`, I know that `test/foo.c` needs to either link or mock `bar.c`. (There are downsides to my approach and I'm doing it pretty naively, but since C is so fast to compile it's mostly fine)

## build
Bootstrap it with `cmake`, and then it self hosts.
```bash
mkdir bootstrap
cmake -S . -B build
./bootstrap/bin/spn build
./build/debug/store/bin/spn build
```

