- finish generating the lockfile properly
- add numeric version to lockfile
- version adapters
- `spn.h`
  - SPN_DEPS()
  - spn_build()
- `spn.lock.h`
  - SPN_VERSION
  - SPN_LOCKS()
- `spn init` -> main.c, (which files get what? between macros and code)
- `spn init --bundled`
  - have to support parse + insert for this case
- `spn bundle main.c` SPN_PROJECT, SPN_BUILD, main.c

- `spn build`
- compile with SPN_PROJECT, grab version and commit
- pull correct spn headers
- compile with SPN_PROJECT + SPN_BUILD + correct headers

- why have them write the build() at all? unless they need to. everything goes in one file.
