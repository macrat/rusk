CFLAGS = $(shell pkg-config --cflags gtk+-3.0 webkit-1.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 webkit-1.0)

.PHONY: all
all: rusk

.PHONY: clean
clean:
	-rm *.o
	-rm rusk

rusk: main.o
	gcc $^ -o $@ ${LDFLAGS}

.c.o:
	gcc -c $^ -o $@ ${CFLAGS}
