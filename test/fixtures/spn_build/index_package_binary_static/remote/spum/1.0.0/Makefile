CC = cc

all: static shared

static: libspum.a

shared: libspum.so

libspum.o: spum.c spum.h
	$(CC) -c spum.c -o libspum.o

libspum.a: libspum.o
	ar rcs libspum.a libspum.o

libspum.so: spum.c spum.h
	$(CC) -shared -fPIC spum.c -o libspum.so
