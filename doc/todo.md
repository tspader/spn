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

would be cool if we could implement this as a package; have an spn.toml that lists all the tools you want installed. and then build.c has a way of querying executables from deps and copying them. but we don't want to allow copying out-of-source, so it'd have to be in ~/.local, which probably isn't what we want.

better to build it into the toml?

# spn update $package
like "cargo update" or "uv lock --update". except we're going to require that you specify a package. read spn_app_add; all we want to do is the part at the bottom, where we make a spn_dep_req_t with the latest version, then run the resolver, prepare. except we want to update the LOCK file, not the project file.

this is a naive approach; what we really would need to do is find the latest version which satisfies the semver version in the project file. the naive approach would, given ^1.2.0 in the project file, allow 3.0.0 to be selected. but it's fine for now.
