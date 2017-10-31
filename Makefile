CC=gcc
CFLAGS=-c -g -O2 -Wall -std=gnu99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -Iinclude
LD=gcc
LDFLAGS=-lpcre

SOURCES=src/parse.c
SOURCES+=src/dllist.c
SOURCES+=src/utils.c
SOURCES+=src/imports.c
SOURCES+=src/mibtree.c
SOURCES+=src/main.c

HEADERS=include/dllist.h
HEADERS+=include/parse.h
HEADERS+=include/imports.h
HEADERS+=include/utils.h
HEADERS+=include/mibtree.h


define SourcesToObjects =
$(addprefix obj/, $(addsuffix .o, $(notdir $(basename $(1)))))
endef

define DefineRule =
$(call SourcesToObjects, $(1)): $(1) Makefile $(HEADERS)
	$$(CC) $$(CFLAGS) $$< -o $$@
endef


.PHONY: clean
.PHONY: all

all: obj parse

obj:
	mkdir -p obj

parse: $(call SourcesToObjects, $(SOURCES))
	$(LD) $(LDFLAGS) -o $@ $^
clean:
	rm -rf obj parse

$(foreach src, $(SOURCES), $(eval $(call DefineRule, $(src))))

