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



# spn run $bin
find the bin with the matching name in the project file. compile it to memory with tcc and run it immediately.

just add SPN_CC_TARGET_RUN and make it a new switch statement. use libtcc's API, like we do to compile recipes. see if we can't just do tcc_set_options() with the one long string provided by spn print (spn_gen..._entries or something, forgot the fn). otherwise, maybe we just need to refactor slightly to produce a sp_da(sp_str_t) of formatted args that we could either set on the process config or iterate and pass to tcc.

unsure if tcc supports more than one libpath?

# set CC for your deps
don't remember if this works. if your profile says to use tcc, your deps should use tcc as well.

need some more validation tho
- static libc => no shared deps
- can i make it easy to compile against old glibc?

# spn remove
first, check the project's deps. if it's not there, SP_FATAL(). load the resolve graph from the lock file; if the package isn't there, you're good to go. just remove it from `app.package.deps`, update `spn.toml`.

if it IS in the lock file, you just need to take care to remove from the lock file anything that was transitively included by that dep

# spn install
like "cargo install" or "uv tool install". add binaries to ~/.local/bin. symlink them in.

would be cool if we could implement this as a package; have an spn.toml that lists all the tools you want installed. and then build.c has a way of querying executables from deps and copying them.
