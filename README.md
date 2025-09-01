<p align="center">
    <img src="asset/github/boon_bane.png" alt="i have no mouse and i must hover the alt text" width="210">
</p>

<p align="center">
  <i>spn: the minimal, (non) package manager for c/++ that nobody asked for. inspired by lazy.nvim and uv</i>
</p>

# Quickstart
Install. (Until I publish a binary, this unfortunately has to build SDL and will take a few minutes)
```bash
curl https://raw.githubusercontent.com/tspader/spn/refs/heads/main/install.sh | sh
```

Initialize a project, add SDL, and compile a program which calls `SDL_Log()`
```bash
> spn init
> spn add sdl3
> spn build
> gcc main.c $(spn print --compiler gcc) -o hello
> ./hello
"spn is a simple, stupid package manager for c inspired by uv and lazy.nvim"
```

# Motivation
If every language in the world runs on C libraries under the hood, why isn't it trivial to pull in C libraries in C projects?

I use C as my daily driver to write almost everything. I write CLI tools, games, simulations, quick scripts. I have done so for years, and I have not encountered a situation where my dependency builds were more complex than a couple lines of shell script and putting binaries and headers in the right place. I shipped a game to Steam, and this remained true.

`spn` is a dead simple package manager which lets you:
- List dependencies in `spn.toml`, add them with `spn add $package`, build them in parallel with `spn build`
- Define package recipes in tiny Makefiles
- Pin dependencies to exact Git commits, automatically check for updates when you build (like `lazy.nvim`)
- Produce compiler flags to consume your dependencies with `spn print`

# Goals
- Build packages eagerly and asynchronously by default; when you build your project, you shouldn't have to wait for dependencies to build unless absolutely necessary.
- Cache local dependency builds
- Support all kinds of libraries:
  - Shared
  - Static
  - Header only (e.g. `stb_image`)
  - Vendored (e.g. `ImGui`)
- Be agnostic to *your* build system and that of your dependencies

# Non-goals
- No transitive dependencies. This is why `spn` can be so simple; it ignores the problem that real package managers solve. In exchange, you get fast, simple, reliable builds of your dependencies.
- Binaries are strictly local only; no binaries are served from the network. Everything is built from source.

# How it works
## your project is a toml file
Your project is just a TOML file. List your dependencies in an array, then optionally configure them.
```toml
[project]
name = 'demo'
deps = ['sdl3']

[deps.sdl3]
options.foo.bar = 69
```

## recipes are makefiles
`spn` searches through its recipe directories (i.e. the one in this repo and any that you added) for `sdl3.mk`. Recipes are just Makefiles that get a few environment variables passed to them:
- `SPN_DIR_PROJECT`: The directory where the package is checked out
- `SPN_DIR_STORE_BIN`: The directory to put binaries
- `SPN_DIR_STORE_INCLUDE`: The directory to put headers
- `SPN_DIR_BUILD`: The out of source build directory you can use to build inside of
- `SPN_OPT_FOO_BAR`: The value of, for example, `deps.sdl3.options.foo.bar` from your `spn.toml` when building `sdl3`

Then, they define two targets, `spn-clone` and `spn-build`. All together, it looks like this.
```make
# recipes/sdl3.mk
HEADERS := $(SPN_DIR_STORE_INCLUDE)/SDL3
BINARY := $(SPN_DIR_STORE_BIN)/libSDL3.so

CMAKE_FLAG_DEFINES := -DCMAKE_BUILD_TYPE=Debug -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_EXAMPLES=OFF
CMAKE_FLAGS := $(CMAKE_FLAG_DEFINES)

.PHONY: spn-clone spn-build

$(SPN_DIR_PROJECT):
	git clone git@github.com:libsdl-org/sdl.git $(SPN_DIR_PROJECT)

$(BINARY):
	cmake -S$(SPN_DIR_PROJECT) -B$(SPN_DIR_BUILD) $(CMAKE_FLAGS)
	cmake --build $(SPN_DIR_BUILD) --parallel
	cp $(SPN_DIR_BUILD)/libSDL3.so $(SPN_DIR_STORE_BIN)

$(HEADERS):
	cp -r $(SPN_DIR_PROJECT)/include/SDL3 $(SPN_DIR_STORE_INCLUDE)

spn-clone: $(SPN_DIR_PROJECT)

spn-build: $(BINARY) $(HEADERS)
```

That's it. Nearly every library I regularly use can be built and packaged with a recipe no more complex than this.

## spn is build system agnostic
When you call `spn build`, it builds all your dependencies in parallel by checking out the correct commit and invoking `spn-build`. After you build, a lock file is created, and your dependencies are pinned to an exact commit.

In your build system, you just call `spn print` to produce the flags needed to include all your packages on your compiler (i.e. `spn print include --compiler gcc` -> `-I/path/to/sdl3/in/store`).

## that's it
Why do builds have to be more complex than this? There are projects for which builds *are* more complicated. But for such projects, `spn` is not the tool for you.


# FAQ
## why wouldn't i just use...
### ...conan?
Conan is objectively an order of magnitude more robust and sophisticated than `spn`. I have used it, quite extensively. It's a pretty good tool, and I'd recommend it for a lot of uses.

But we're not making corporate software. We don't need all the machinery that Conan provides. Their recipes are a lot more complicated. You can't eject from it without completely remaking your build system. If you need to hack a recipe to do something, it takes a lot more work to understand how everything works. Compare, for example, the recipe for `argparse`, a very small C library for which we both provide recipes:
- https://github.com/conan-io/conan-center-index/tree/master/recipes/argparse
- https://github.com/tspader/spn/blob/main/asset/recipes/argparse.mk

Note that I am not claiming to be better than Conan. Merely that the tools solve very different problems, and if you have the problems that Conan aims to solve, you are not the target audience.

### ..cmake squad?
There are tools that have very similar goals to `spn` but which use CMake:
- https://hunter.readthedocs.io/en/latest/
- https://github.com/cpm-cmake/CPM.cmake

Unfortunately, CMake is an unholy dumpster fire of incompatible versions, baffling DSL, and incomprehensible package system. What's worse is that with enough effort a CMake project *does* tend to Just Work on most any system. This, like fruit flies to a rotting banana, entices developers and bades them forth. Even I, with such a strong immune response after dozens of infections, am hooked in approximately once per year.

Several days later I emerge from my fugue state, only to read the incantations now littering my machine. Did I really write that? My project just needs to call `gcc` a few times -- why do I have seven `CMakeLists.txt` in this repository?

I shake myself off and promise myself that I will not fall for her siren song again.

### ...git submodules?
`spn` isn't much more complicated than this. But why write the same stupid build script for every project?

## why do you use C?
C is portable, easy to compile, easy to debug, will compile in a hundred years barring societal collapse. The main things C is missing are:
- [A package manager](https://github.com/tspader/spn)
- [A standard library](https://github.com/tspader/sp)

# Roadmap
`spn` is very much an MVP. It's missing a lot of core features. PRs are very welcome!
- Robust handling of user recipes (e.g. system-wide recipe repos; specify via git URL and have `spn` keep it up to date; per-project recipe directories)
- Build profiles (i.e. different sets of options which can be selected with `--profile`
- Windows support
- Lots of recipes


