CC=gcc
CFLAGS=-Wall -Wextra
LDFLAGS=-ldl

#used to remove the need for export LD_LIBRARY_PATH=.
RPATH=-Wl,-rpath=./

all: stnc

stnc: stnc.c
	$(CC) stnc.c -o stnc

clean:
	rm -f *.o stnc