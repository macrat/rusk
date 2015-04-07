CFLAGS = --std=c99 $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-3.0 sqlite3)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 webkit2gtk-3.0 sqlite3)

.PHONY: all
all: rusk rusk-dl

.PHONY: clean
clean:
	-rm *.o
	-rm rusk rusk-dl

rusk: main.o
	gcc $^ -o $@ ${LDFLAGS}

rusk-dl: downloader.o
	gcc $^ -o $@ ${LDFLAGS}

.c.o:
	gcc -c $^ -o $@ ${CFLAGS}
