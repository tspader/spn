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

# spn update $package
like "cargo update" or "uv lock --update". except we're going to require that you specify a package. read spn_app_add; all we want to do is the part at the bottom, where we make a spn_dep_req_t with the latest version, then run the resolver, prepare. except we want to update the LOCK file, not the project file.

this is a naive approach; what we really would need to do is find the latest version which satisfies the semver version in the project file. the naive approach would, given ^1.2.0 in the project file, allow 3.0.0 to be selected. but it's fine for now.
