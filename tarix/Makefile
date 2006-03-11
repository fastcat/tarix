# Makefile for tarix

TARGET=bin/tarix

MAIN_SRC=src/tarix.c
LIB_SRCS=src/create_index.c src/extract_files.c src/portability.c \
	src/tstream.c src/crc32.c src/ts_util.c
SOURCES=${MAIN_SRC} ${LIB_SRCS}
OBJECTS=$(patsubst src/%.c,obj/%.o,${SOURCES})
LIB_OBJS=$(patsubst src/%.c,obj/%.o,${LIB_SRCS})
DEPS=$(patsubst src/%.c,obj/%.d,${SOURCES})

T_SOURCES=test/t_tws.c test/t_trs.c
T_OBJECTS=$(patsubst test/%.c,obj/test/%.o,${T_SOURCES})
T_DEPS=$(patsubst test/%.c,obj/test/%.d,${T_SOURCES})
T_TARGETS=$(patsubst test/%.c,bin/test/%,${T_SOURCES})

CPPFLAGS=-Isrc -D_GNU_SOURCE
CFLAGS=-Wall -Werror -g
LDFLAGS=-lz
CC=gcc
INSTBASE=/usr/local

.PHONY :: all dep build_test

all : ${TARGET} build_test

dep : ${DEPS}

build_test: bin/test/.d ${T_TARGETS}

test-%: bin/test/t_%
	@echo Testing: $*
	@if [ -f test/t_$*.sh ]; then test/t_$*.sh ; else $< ; fi
	@echo Testing: $* Done

test: $(patsubst bin/test/t_%,test-%,${T_TARGETS})

${OBJECTS} ${DEPS} : obj/.d bin/.d

${T_OBJECTS} ${T_DEPS} : obj/test/.d bin/test/.d

bin/%: obj/%.o ${LIB_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

bin/test/%: obj/test/%.o ${LIB_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

%/.d :
	@mkdir -p $*
	@touch $@

obj/%.o : src/%.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

obj/test/%.o : test/%.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

obj/%.d : src/%.c
	${CC} ${CPPFLAGS} -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/\1.o obj\/\1.d : /' >$@

obj/test/%.d : test/%.c
	${CC} ${CPPFLAGS} -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/test\/\1.o obj\/test\/\1.d : /' >$@

install: all
	install ${TARGET} ${INSTBASE}/bin

clean:
	rm -rf obj/ out.tarix

distclean: clean
	rm -rf bin/

-include ${DEPS} ${T_DEPS}
