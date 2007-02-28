# Makefile for tarix

TARGETS=bin/tarix bin/fuse_tarix

MAIN_SRC=src/tarix.c src/fuse_tarix.c
LIB_SRCS=src/create_index.c src/extract_files.c src/portability.c \
	src/tstream.c src/crc32.c src/ts_util.c \
	src/lineloop.c src/index_parser.c
SOURCES=${MAIN_SRC} ${LIB_SRCS}
OBJECTS=$(patsubst src/%.c,obj/%.o,${SOURCES})
LIB_OBJS=$(patsubst src/%.c,obj/%.o,${LIB_SRCS})
DEPS=$(patsubst src/%.c,obj/%.d,${SOURCES})

T_SOURCES=test/t_tws.c test/t_trs.c test/t_tsk.c
T_OBJECTS=$(patsubst test/%.c,obj/test/%.o,${T_SOURCES})
T_DEPS=$(patsubst test/%.c,obj/test/%.d,${T_SOURCES})
T_TARGETS=$(patsubst test/%.c,bin/test/%,${T_SOURCES})
TESTS=$(patsubst test/%.c,test-%,${T_SOURCES})

CPPFLAGS=-Isrc -D_GNU_SOURCE
CFLAGS=-Wall -Werror -g
CPPFLAGS_fuse_tarix:=$(shell pkg-config fuse --cflags)
# for cygwin, export LDFLAGS=-L/usr/bin
LDFLAGS+=-lz
LDFLAGS_fuse_tarix:=$(shell pkg-config fuse --libs)
CC=gcc
INSTBASE=/usr/local

.PHONY :: all dep build_test test ${TESTS}

all : ${TARGETS}

dep : ${DEPS}

build_test: bin/test/.d ${T_TARGETS}

test-%: bin/test/t_%
	@echo Testing: $*
	@if [ -f test/t_$*.sh ]; then test/t_$*.sh ; else $< ; fi
	@echo Testing: $* Done

test: build_test $(patsubst bin/test/t_%,test-%,${T_TARGETS})

${OBJECTS} ${DEPS} : obj/.d bin/.d

${T_OBJECTS} ${T_DEPS} : obj/test/.d bin/test/.d

bin/%: obj/%.o ${LIB_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} $(LDFLAGS_$(*)) -o $@ $^

bin/test/%: obj/test/%.o ${LIB_OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $^

%/.d :
	@mkdir -p $*
	@touch $@

obj/%.o : src/%.c
	${CC} ${CPPFLAGS} $(CPPFLAGS_$(*)) ${CFLAGS} $(CFLAGS_$(*)) -c $< -o $@

obj/test/%.o : test/%.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

obj/%.d : src/%.c
	${CC} ${CPPFLAGS} $(CPPFLAGS_$(*)) -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/\1.o obj\/\1.d : /' >$@

obj/test/%.d : test/%.c
	${CC} ${CPPFLAGS} -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/test\/\1.o obj\/test\/\1.d : /' >$@

install: all
	install ${TARGETS} ${INSTBASE}/bin

clean:
	rm -rf obj/ out.tarix

distclean: clean
	rm -rf bin/

-include ${DEPS} ${T_DEPS}
