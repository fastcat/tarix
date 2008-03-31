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

TESTS=$(patsubst test/%.sh,%,$(wildcard test/*.sh))
_WANT_T_SOURCES=$(patsubst %,test/%.c,${TESTS})
_HAVE_T_SOURCES=$(wildcard test/*.c)
T_SOURCES=$(filter ${_HAVE_T_SOURCES},${_WANT_T_SOURCES})
T_OBJECTS=$(patsubst test/%.c,obj/test/%.o,${T_SOURCES})
T_DEPS=$(patsubst test/%.c,obj/test/%.d,${T_SOURCES})
T_TARGETS=$(patsubst test/%.c,bin/test/%,${T_SOURCES})

CPPFLAGS=-Isrc -D_GNU_SOURCE
# some warnings only can be shown with -O1
ifdef DEBUG
CFLAGS_O=
else
CFLAGS_O=-O1
endif
CFLAGS=-Wall -Werror -g -std=gnu99 $(CFLAGS_O)
CPPFLAGS_fuse_tarix:=$(shell pkg-config fuse --cflags) $(shell pkg-config glib-2.0 --cflags)
# for cygwin, export LDFLAGS=-L/usr/bin
LDFLAGS+=-lz
LDFLAGS_fuse_tarix:=$(shell pkg-config fuse --libs) $(shell pkg-config glib-2.0 --libs)
CC=gcc
INSTBASE=/usr/local

.PHONY :: all dep build_test test ${TESTS}

all : ${TARGETS}

dep : ${DEPS}

build_test: ${TARGETS} bin/test/.d ${T_TARGETS}

test-%: build_test test/%.sh
	@printf "Test: %-30s ..." $*
	@if test/$*.sh &>bin/test/$*.log ; then \
		echo " OK" ; \
	else \
		echo FAILED ; \
		cat bin/test/$*.log ; \
		exit 1 ; \
	fi

test: build_test $(patsubst %,test-%,${TESTS})
	@echo All tests appear to have passed

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
