`spn link $package [--bin, --lib] [--mode (hard, symbolic, copy)]`
- enumerate `[[bin]]` entries and `[lib]`; collect binaries or libs as appropriate; copy from $store/bin/$file or $store/lib/$file
- use the sp.h function that turns lib name into platform appropriate file (e.g. .dll vs .so vs .dylib). i forget the name. sp_os_lib_kind...?
- obviously hardlink/symlink/copy as appropriate.
- implement hardlink/symlink as sp_os_link(path, mode) so we can merge it upstream when we're done
