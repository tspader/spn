.PHONY: all clean bootstrap

all: bootstrap/bin/spn

bootstrap/external:
	@mkdir -p bootstrap/external

bootstrap/external/argparse: bootstrap/external
	@if [ ! -d "$@" ]; then \
		echo "Cloning argparse..."; \
		git clone https://github.com/tspader/argparse.git $@; \
	fi

bootstrap/external/sp: bootstrap/external
	@if [ ! -d "$@" ]; then \
		echo "Cloning sp..."; \
		git clone https://github.com/tspader/sp.git $@; \
	fi

bootstrap/external/toml: bootstrap/external
	@if [ ! -d "$@" ]; then \
		echo "Cloning toml..."; \
		git clone https://github.com/tspader/toml.git $@; \
	fi

bootstrap/external/tinycc: bootstrap/external
	@if [ ! -d "$@" ]; then \
		echo "Cloning tinycc..."; \
		git clone https://github.com/tinycc/tinycc.git $@; \
	fi

bootstrap/lib/libtcc.a: bootstrap/external/tinycc
	@if [ ! -f "$@" ]; then echo "Building tinycc..."; cd bootstrap/external/tinycc && ./configure --enable-static --prefix=$(PWD)/bootstrap && $(MAKE) && $(MAKE) install; fi

bootstrap/include:
	@mkdir -p bootstrap/include

bootstrap/include/%.h: bootstrap/external/%/spn.h | bootstrap/include
	@cp $< $@

bootstrap/include/sp.h: bootstrap/external/sp | bootstrap/include
	@cp bootstrap/external/sp/sp.h $@

bootstrap/include/toml.h: bootstrap/external/toml | bootstrap/include
	@cp bootstrap/external/toml/toml.h $@

bootstrap/include/libtcc.h: bootstrap/lib/libtcc.a | bootstrap/include
	@cp bootstrap/external/tinycc/libtcc.h $@

bootstrap/include/argparse.h: bootstrap/external/argparse | bootstrap/include
	@cp bootstrap/external/argparse/argparse.h $@

bootstrap/bin:
	@mkdir -p bootstrap/bin

bootstrap/bin/spn: source/spn.c bootstrap/lib/libtcc.a bootstrap/include/sp.h bootstrap/include/toml.h bootstrap/include/libtcc.h bootstrap/include/argparse.h | bootstrap/bin
	@echo "Building spn..."
	gcc -g -static -o $@ $< -Iinclude -Ibootstrap/include bootstrap/lib/libtcc.a

bootstrap: bootstrap/bin/spn

install: bootstrap/bin/spn
	@mkdir -p $(HOME)/.local/bin && cp bootstrap/bin/spn $(HOME)/.local/bin/

uninstall:
	@rm -f $(HOME)/.local/bin/spn

clean:
	rm -rf bootstrap/external
	rm -rf bootstrap/lib
	rm -rf bootstrap/include
	rm -rf bootstrap/bin

c:
	rm bootstrap/bin/spn
