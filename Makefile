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
	@if [ ! -d bootstrap/external/tinycc ]; then git clone https://github.com/tinycc/tinycc.git bootstrap/external/tinycc; fi

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

bootstrap/bin/spn: source/spn.c bootstrap/lib/libtcc.a bootstrap/include/sp.h bootstrap/include/toml.h bootstrap/include/libtcc.h bootstrap/include/argparse.h
	@mkdir -p bootstrap/bin
	gcc -g -o $@ source/spn.c -Iinclude -Ibootstrap/include bootstrap/lib/libtcc.a -lm


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
