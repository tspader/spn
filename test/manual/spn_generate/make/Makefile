all:
	cp ../main.c ../spn.toml .
	spn build
	spn generate --generator make --path .
	$(MAKE) build

-include spn.mk

CC := gcc
CFLAGS := -Wall

build: main
	./main

main: main.c
	$(CC) $(CFLAGS) $< -o $@ $(SPN_FLAGS)

clean:
	rm -rf main spn.mk spn.lock build main.c spn.toml

.PHONY: all build clean
