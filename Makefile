TARGET=bin/tarix
SOURCES=src/tarix.c src/create_index.c src/extract_files.c
OBJECTS=$(patsubst src/%.c,obj/%.o,${SOURCES})
DEPS=$(patsubst src/%.c,obj/%.d,${SOURCES})
CPPFLAGS=-Isrc -D_GNU_SOURCE
CFLAGS=-Wall -g
CC=gcc
INSTBASE=/usr/local

all: ${TARGET}

dep: ${DEPS}

${TARGET}: bin/.d ${OBJECTS}
	${CC} ${CFLAGS} -o ${TARGET} ${OBJECTS}

${OBJECTS} ${DEPS}: obj/.d

%/.d:
	@mkdir -p $*
	@touch $@

obj/%.o: src/%.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

obj/%.d: src/%.c
	${CC} ${CPPFLAGS} -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/\1.o obj\/\1.d : /' >$@

install: all
	install ${TARGET} ${INSTBASE}/bin

clean:
	rm -rf obj/ out.tarix

distclean: clean
	rm -rf bin/

-include ${DEPS}
