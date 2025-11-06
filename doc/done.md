# define profiles in spn.toml
real simple:
```toml
[[profile]]
name = "tcc"
cc = "tcc"
libc = "static"
language = "c99"
mode = "release"
```

just need utilities for string <-> enum where we don't already have em.

when the user defines ANY profiles, we won't define the automatic debug/release profiles for them. we need to make sure that any time we use a default profile that it isn't hardcoded to "debug" or whatever -- we should mark the default profile when we load spn.toml (either one of ours or the first one we loaded from the user).

we also need to mark the profile on spn_bin_t as sp_opt(). if it's not explicitly defined, it should be a nullopt, and we should use the default profile. moreover, we need to ensure that we don't write the profile back to the toml when we call spn_update_project_toml() if the profile was nullopt.
