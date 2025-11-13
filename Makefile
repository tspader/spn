.PHONY: all clean install uninstall clone headers

all: bootstrap/bin/spn

###########
## CLONE ##
###########
clone:
	@mkdir -p bootstrap/external
	git clone https://github.com/tspader/argparse.git bootstrap/external/argparse
	git clone https://github.com/tspader/sp.git bootstrap/external/sp
	git clone https://github.com/tspader/toml.git bootstrap/external/toml
	git clone https://github.com/tinycc/tinycc.git bootstrap/external/tinycc

bootstrap/external/argparse bootstrap/external/sp bootstrap/external/toml bootstrap/external/tinycc: clone


#############
## HEADERS ##
#############
headers: bootstrap/external/sp bootstrap/external/toml bootstrap/external/argparse bootstrap/lib/libtcc.a
	@mkdir -p bootstrap/include
	cp bootstrap/external/sp/sp.h bootstrap/include/sp.h
	cp bootstrap/external/toml/toml.h bootstrap/include/toml.h
	cp bootstrap/external/argparse/argparse.h bootstrap/include/argparse.h
	cp bootstrap/external/tinycc/libtcc.h bootstrap/include/libtcc.h

bootstrap/include/sp.h bootstrap/include/toml.h bootstrap/include/argparse.h bootstrap/include/libtcc.h: headers


##############
## BINARIES ##
##############
bootstrap/lib/libtcc.a: bootstrap/external/tinycc
	cd bootstrap/external/tinycc && ./configure --enable-static --prefix=$(PWD)/bootstrap && $(MAKE) && $(MAKE) install

bootstrap/bin/spn: source/spn.c bootstrap/lib/libtcc.a bootstrap/include/sp.h bootstrap/include/toml.h bootstrap/include/libtcc.h bootstrap/include/argparse.h
	@mkdir -p bootstrap/bin
	gcc -g -o $@ source/spn.c -Iinclude -Ibootstrap/include bootstrap/lib/libtcc.a


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
