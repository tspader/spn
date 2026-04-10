## overview
The missing build tool and package manager for C.

Zack, I wrote a README just for you. Plus pushed up fifty-something WIP commits from a giant rewrite branch. It does not, therefore, build. It'll all be rebased away later.

I'm trying a new project structure, if it looks weird. Every module (rough and conceptual, not real modules) define:
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

