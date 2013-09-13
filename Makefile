# Makefile for tarix

TARGETS:=bin/tarix bin/fuse_tarix
DISABLED_TARGETS:=
MISSING_DEPS:=

CPPFLAGS_FUSE:=$(strip $(shell pkg-config fuse --cflags --silence-errors))
CPPFLAGS_GLIB:=$(strip $(shell pkg-config glib-2.0 --cflags --silence-errors))
LDFLAGS_FUSE:=$(strip $(shell pkg-config fuse --libs --silence-errors))
LDFLAGS_GLIB:=$(strip $(shell pkg-config glib-2.0 --libs --silence-errors))

# disable fuse if it's not available
ifeq (${CPPFLAGS_FUSE},)
	TARGETS:=$(filter-out bin/fuse%,${TARGETS})
	DISABLED_TARGETS+=disabled-fuse_tarix
	MISSING_DEPS+=missing-fuse
endif
ifeq (${CPPFLAGS_GLIB},)
	TARGETS:=$(filter-out bin/fuse%,${TARGETS})
	DISABLED_TARGETS+=disabled-fuse_tarix
	MISSING_DEPS+=missing-glib
endif

MAIN_SRC=$(patsubst bin/%,src/%.c,${TARGETS})
LIB_SRCS=src/create_index.c src/extract_files.c src/portability.c \
	src/tstream.c src/crc32.c src/ts_util.c \
	src/lineloop.c src/index_parser.c src/files_list.c
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

CPPFLAGS=-I. -Isrc -D_GNU_SOURCE
# some warnings only can be shown with -O1
ifeq (${DEBUG},1)
CFLAGS_O=-O0 -g
else
CFLAGS_O=-O3
endif
OPTCFLAGS?=
CFLAGS=-Wall -Werror -std=gnu99 $(CFLAGS_O) $(OPTCFLAGS)
CPPFLAGS_fuse_tarix:= ${CPPFLAGS_FUSE} ${CPPFLAGS_GLIB}
LDFLAGS+=-lz
LDFLAGS_fuse_tarix:=${LDFLAGS_FUSE} ${LDFLAGS_GLIB}
CC?=gcc
INSTBASE?=/usr/local

.PHONY :: all dep build_test test debuginfo ${TESTS}

all : ${TARGETS} ${MISSING_DEPS} ${DISABLED_TARGETS}

dep : ${DEPS}

build_test: ${TARGETS} bin/test/.d ${T_TARGETS}

test-%: build_test test/%.sh
	@printf "Test: %-30s ..." $*
	@if test/$*.sh >bin/test/$*.log 2>&1 ; then \
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
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS} $(LDFLAGS_$(*))

bin/test/%: obj/test/%.o ${LIB_OBJS}
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

config.h:
	@echo '#include <sys/mtio.h>' > .test.h
	@if ${CC} -E ${CPPFLAGS} .test.h 1>/dev/null 2>&1 ; then \
		echo '#define HAVE_MTIO_H 1' >> config.h ; \
	else \
		echo '#ifdef HAVE_MTIO_H' >> config.h ; \
		echo '# undef HAVE_MTIO_H' >> config.h ; \
		echo '#endif' >> config.h ; \
	fi
	@rm -f .test.h

%/.d :
	@mkdir -p $*
	@touch $@

obj/%.o : src/%.c config.h
	${CC} ${CPPFLAGS} $(CPPFLAGS_$(*)) ${CFLAGS} $(CFLAGS_$(*)) -c $< -o $@

obj/test/%.o : test/%.c
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

obj/%.d : src/%.c config.h
	${CC} ${CPPFLAGS} $(CPPFLAGS_$(*)) -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/\1.o obj\/\1.d : /' >$@

obj/test/%.d : test/%.c
	${CC} ${CPPFLAGS} -MM $< | sed -e 's/^\(.*\)\.o[ :]*/obj\/test\/\1.o obj\/test\/\1.d : /' >$@

install: all
	install ${TARGETS} ${INSTBASE}/bin

clean:
	rm -rf obj/ out.tarix config.h

distclean: clean
	rm -rf bin/

debuginfo:
	@echo TARGETS=${TARGETS}
	@echo DEPS=${DEPS}

disabled-%:
	@echo DISABLED: $*
	@echo "    Not building $* because of missing dependencies"

missing-%:
	@echo "MISSING: $*"
	@echo "    Some components were not built because the $* dependency"
	@echo "    is unavailable or could not be found with pkg-config"

-include ${DEPS} ${T_DEPS}
