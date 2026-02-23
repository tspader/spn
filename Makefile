.PHONY: all clean install uninstall clone

all: bootstrap/bin/spn

###########
## CLONE ##
###########
clone:
	@mkdir -p bootstrap/external
	@if [ ! -d bootstrap/external/argparse ]; then git clone https://github.com/tspader/argparse.git bootstrap/external/argparse; fi
	@if [ ! -d bootstrap/external/sp ]; then git clone https://github.com/tspader/sp.git bootstrap/external/sp; fi
	@if [ ! -d bootstrap/external/toml ]; then git clone https://github.com/tspader/toml.git bootstrap/external/toml; fi
	@if [ ! -d bootstrap/external/tinycc ]; then git clone https://github.com/tspader/tinycc.git bootstrap/external/tinycc && git -C bootstrap/external/tinycc checkout spn; fi

bootstrap/external/argparse bootstrap/external/sp bootstrap/external/toml bootstrap/external/tinycc: clone


#############
## HEADERS ##
#############
bootstrap/include/sp.h: bootstrap/external/sp
	@mkdir -p bootstrap/include
	@cp bootstrap/external/sp/sp.h $@

bootstrap/include/toml.h: bootstrap/external/toml
	@mkdir -p bootstrap/include
	@cp bootstrap/external/toml/toml.h $@

bootstrap/include/argparse.h: bootstrap/external/argparse
	@mkdir -p bootstrap/include
	@cp bootstrap/external/argparse/argparse.h $@

bootstrap/include/libtcc.h: bootstrap/lib/libtcc.a
	@mkdir -p bootstrap/include
	@cp bootstrap/external/tinycc/libtcc.h $@


##############
## BINARIES ##
##############
bootstrap/lib/libtcc.a: bootstrap/external/tinycc
	@if [ ! -f bootstrap/lib/libtcc.a ]; then cd bootstrap/external/tinycc && ./configure --enable-static --prefix=$(PWD)/bootstrap && $(MAKE) && $(MAKE) install; fi

bootstrap/bin/embed: tools/embed.c bootstrap/include/sp.h
	@mkdir -p bootstrap/bin
	gcc -g -o $@ tools/embed.c -Ibootstrap/include -lm

bootstrap/lib/spn.embed.o bootstrap/include/spn.embed.h: bootstrap/bin/embed bootstrap/lib/libtcc.a
	./bootstrap/bin/embed bootstrap/lib/tcc bootstrap/lib/spn.embed.o bootstrap/include/spn.embed.h

bootstrap/bin/spn: source/spn.c source/intern.c source/log.c source/option.c source/registry.c source/pkg.c source/lock.c source/autoconf.c source/make.c source/cmake.c source/profile.c source/cc.c source/gen.c source/target.c source/target_filter.c source/resolve.c source/semver.c source/external/git.c source/external/tcc.c source/ordered_map.c source/spinner.c source/stoml.c source/pty.c source/sp/str.c source/sp/color.c source/sp/os.c source/sp/ps.c source/sp/io.c bootstrap/lib/libtcc.a bootstrap/lib/spn.embed.o bootstrap/include/spn.embed.h bootstrap/include/sp.h bootstrap/include/toml.h bootstrap/include/libtcc.h bootstrap/include/argparse.h
	@mkdir -p bootstrap/bin
	gcc -g -o $@ source/spn.c source/intern.c source/log.c source/option.c source/registry.c source/pkg.c source/lock.c source/autoconf.c source/make.c source/cmake.c source/profile.c source/cc.c source/gen.c source/target.c source/target_filter.c source/resolve.c source/semver.c source/external/git.c source/external/tcc.c source/ordered_map.c source/spinner.c source/stoml.c source/pty.c source/sp/str.c source/sp/color.c source/sp/os.c source/sp/ps.c source/sp/io.c -Iinclude -Ibootstrap/include -lm bootstrap/lib/libtcc.a bootstrap/lib/spn.embed.o


#############
## PHONIES ##
#############
install: bootstrap/bin/spn
	@mkdir -p $(HOME)/.local/bin
	cp bootstrap/bin/spn $(HOME)/.local/bin/

uninstall:
	rm -f $(HOME)/.local/bin/spn

clean:
	rm -rf bootstrap
