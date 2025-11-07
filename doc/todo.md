# spn build --profile profile
we just need to add a string arg via argparse; SP_FATAL() if we can't find the profile.

# spn tool
- `spn tool install foo` -> try to find package for `foo` in registries (do not try to load an spn.toml)
- `spn tool install .` -> look in `.` for `spn.toml` and use that as the package
- "install a tool" means to build a package and add any `[[bin]]` entries to ~/.local/bin
- we'll keep an `spn.toml` in ~/.local/share/spn/tools
- we'll build just as if we were running `spn build` builds all the tools into the spn cache, just like normal (user doesn't run this manually, just as a point of reference)
- we need to copy the binary + any shared libraries into the right place; for now, we can ignore shared libraries. we'll use `spn link`
- `spn tool list` just list the deps used by that internal spn.toml, use the same code as for spn list.
- `spn tool run $package` run the first binary provided by a package
- `spn tool upgrade $package` equivalent of `spn upgrade` on internal spn.toml

11/6: note that we should NOT literally shell out with a subprocess; that's awful. we'll use the right programmatic interfaces. the problem is that a lot of code is hardcoded to assume one spn_app_t / one top level package at once. i rewrote a bunch of it today to be better. not sure if it's 100% there yet. creating a second app is still a little funky? but it should be close. just gotta keep going with the implementation (spn init + spn add + spn upgrade + spn build + spn link). shouldnt need any custom code in sp.c; `spn link` should be able to link anywhere we want + just link a subset of files + link flat if we want.

# spn link
`spn link $package [--bin, --lib] [--mode (hard, symbolic, copy)]`
- enumerate `[[bin]]` entries and `[lib]`; collect binaries or libs as appropriate; copy from $store/bin/$file or $store/lib/$file
- use the sp.h function that turns lib name into platform appropriate file (e.g. .dll vs .so vs .dylib). i forget the name. sp_os_lib_kind...?
- obviously hardlink/symlink/copy as appropriate.
- implement hardlink/symlink as sp_os_link(path, mode) so we can merge it upstream when we're done

# spn list
- nicely formatted list of packages + versions
- SP_LOG("{:fg brightcyan}: {}", SP_FMT_STR(sp_str_pad(name, width)), SP_FMT_STR(sp_semver_to_str(version))), roughly
- be sure to pad to the longest dep name; iterate once to gather longest name, then iterate again to print

# spn -c / --directory OR -f / --file
- specify a directory to look for spn.toml
- used to work, need to verify if it still does and fix if not
- -f / --file looks for the file itself


# spn run $bin
find the bin with the matching name in the project file. compile it to memory with tcc and run it immediately.

just add SPN_CC_TARGET_RUN and make it a new switch statement. use libtcc's API, like we do to compile recipes. see if we can't just do tcc_set_options() with the one long string provided by spn print (spn_gen..._entries or something, forgot the fn). otherwise, maybe we just need to refactor slightly to produce a sp_da(sp_str_t) of formatted args that we could either set on the process config or iterate and pass to tcc.

basically the engineering here is to figure out whether we should pass compiler args as one blob to tcc_set_options or try to use the actual tcc API

unsure if tcc supports more than one libpath? or if its even needed? let's just say that this only works with static deps rn

dont duplicate too much code from spn build

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

# convert other help strings to helper struct
spn_cli_usage_t, i think. expand the struct or make a similar one as needed for comamnds that don't have subcommands? also need to make it a little better (e.g. add examples)
