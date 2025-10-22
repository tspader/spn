- static failures:
  - freetype (need system libs, then need brotli + bzip, just configuration)
  - ggml (same thing as whisper, need to configure backend)
  - whisper (not vendoring example)

- windows
  - spn ls
  - builds sp.h OK in the cache but $(spn print) doesn't work

- add project directory to path (-C or -f means it might not necessarily be); but you want to name your project file "spn.lua", so idk how `require('spn`)` will work
- `spn ls luajit` is better than `--package luajit`
- C-c has to work
- maximum ergonomics for day one usage
```bash
spn init
spn add sdl3
spn build
spn run main.c
```
(is there a way to embed spn.lua in anything? could they pass it to us?)

- nuke SDL
  - sp_ps_*
    - sp_ps_apply_env
  - sp_io_*
  - sp_os_remove_file

# C build ecosystem
## packagers
not a build system (i.e. it isn't generating compiler switches). instead, solves problems like: where are your dependencies installed? what are your outputs? where do they go?
- makepkg / PKGBUILD
- rpm
- dpkg
- brew
- conan

## executors
builds a dependency graph of the files needed to produce some output; generally, no special knowledge of C libraries or building them
- make
- ninja
- msbuild

## generators
generates project files; handles finding your dependencies, configuring your build (e.g. include directories) in a build system agnostic way. you describe your build in their language, and it can creates a project file for a build system.

sometimes this is to generate a project for a platform-appropriate executor (e.g. make on posix, msbuild on windows). sometimes this is to figure out what the machine supports and generate a project file that represents that (e.g. nvidia GPU, so use CUDA)
- cmake: insane DSL, incompatibility between versions, constantly deprecated, 100 ways to do things, one of the worst tools ever written; unfortunately, everyone uses it + it pretty much works
- meson: cmake that's not insane
- autotools
  - autoconf: generates the `./configure` script from `configure.ac`; this is the thing which compiles a bunch of tiny test programs to test system capabilities
  - libtool; handles building and linking shared vs. static libraries portably across platforms
  - these are all insane and you should never use unless forced

## mixed
- xmake: generator + executor; prefers to just grab binaries for your deps + lets you specify your project in lua in the same way you would in cmake (e.g. build an executable, include so-and-so directory)
- bazel: generator + executor; everything is totally hermetic, expects you to build from source everything short of the kernel and libc. designed for google where they use huge monorepos

- i want to build all the recipes with tcc on the fly instead of using lua
- i also want to build spn itself using tcc to avoid ABI incompatibility between spn and recipes
    - if i can't, i can always generate functions that return the offset of struct members of the project descriptor struct instead of using the struct directly
- had to build/install tcc from source because arch package is outdated and breaks with link.h
- just replacing gcc with tcc in the makefile errors because you aren't linking to libgcc
- so i tried building all my deps with tcc by:
    - set CC := tcc in the makefile
    - set tools.cc = 'tcc' in build.lua
    - export CC=tcc in the shell before doing anything
- SDL failed to build because:
    - shared builds require -Wl,--version-script=..., which is just a way to list symbols you want exported. tcc does not support this
    - assembly stuff is generally kind of broken; need to define STBI_NO_SIMD in CMAKE_C_FLAGS + patch SDL to not use a Q constraint
    - after that it builds (as a static library)

