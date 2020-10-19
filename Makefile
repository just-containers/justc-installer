#
# This Makefile requires GNU make.
#
# Do not make changes here.
# Use the included .mak files.
#

it: all

make_need := 3.81
ifeq "" "$(strip $(filter $(make_need), $(firstword $(sort $(make_need) $(MAKE_VERSION)))))"
fail := $(error Your make ($(MAKE_VERSION)) is too old. You need $(make_need) or newer)
endif

CC = $(error Please use ./configure first)

STATIC_LIBS :=
LIB_DEFS :=

define library_definition
LIB$(firstword $(subst =, ,$(1))) := lib$(lastword $(subst =, ,$(1))).a.xyzzy
STATIC_LIBS += lib$(lastword $(subst =, ,$(1))).a.xyzzy
endef

-include config.mak
include package/targets.mak

$(foreach var,$(LIB_DEFS),$(eval $(call library_definition,$(var))))

include package/deps.mak

version_m := $(basename $(version))
version_M := $(basename $(version_m))
version_l := $(basename $(version_M))
CPPFLAGS_ALL := $(CPPFLAGS_AUTO) $(CPPFLAGS)
CFLAGS_ALL := $(CFLAGS_AUTO) $(CFLAGS)
ifeq ($(strip $(STATIC_LIBS_ARE_PIC)),)
CFLAGS_SHARED := -fPIC
else
CFLAGS_SHARED :=
endif
LDFLAGS_ALL := $(LDFLAGS_AUTO) $(LDFLAGS)
REALCC = $(CROSS_COMPILE)$(CC)
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
STRIP := $(CROSS_COMPILE)strip
INSTALL := ./tools/install.sh

ALL_LIBS := $(SHARED_LIBS) $(STATIC_LIBS) $(INTERNAL_LIBS)
ALL_INCLUDES := $(wildcard src/include/$(package)/*.h)

all: $(ALL_LIBS) $(ALL_INCLUDES)

clean:
	@exec rm -f $(ALL_LIBS) $(wildcard src/*/*.o src/*/*.lo) $(EXTRA_TARGETS)

distclean: clean
	@exec rm -f config.mak src/include/$(package)/config.h

tgz: distclean
	@. package/info && \
	rm -rf /tmp/$$package-$$version && \
	cp -a . /tmp/$$package-$$version && \
	cd /tmp && \
	tar -zpcv --owner=0 --group=0 --numeric-owner --exclude=.travis* --exclude=dist --exclude=tools/cross --exclude=tools/downloads --exclude=tools/travis* --exclude=tools/keys* --exclude=.git* -f /tmp/$$package-$$version.tar.gz $$package-$$version && \
	exec rm -rf /tmp/$$package-$$version

strip: $(ALL_LIBS)
ifneq ($(strip $(STATIC_LIBS)),)
	exec $(STRIP) -x -R .note -R .comment -R .note.GNU-stack $(STATIC_LIBS)
endif

install: install-lib install-include
install-lib: $(STATIC_LIBS:lib%.a.xyzzy=$(DESTDIR)$(libdir)/lib%.a)
install-include: $(ALL_INCLUDES:src/include/$(package)/%.h=$(DESTDIR)$(includedir)/$(package)/%.h)

ifneq ($(exthome),)

$(DESTDIR)$(exthome): $(DESTDIR)$(home)
	exec $(INSTALL) -l $(notdir $(home)) $(DESTDIR)$(exthome)

update: $(DESTDIR)$(exthome)

global-links: $(DESTDIR)$(exthome) $(SHARED_LIBS:lib%.so.xyzzy=$(DESTDIR)$(sproot)/library.so/lib%.so.$(version_M))

$(DESTDIR)$(sproot)/command/%: $(DESTDIR)$(home)/command/%
	exec $(INSTALL) -D -l ..$(subst $(sproot),,$(exthome))/command/$(<F) $@

.PHONY: update global-links

endif

$(DESTDIR)$(libdir)/lib%.a: lib%.a.xyzzy
	exec $(INSTALL) -D -m 644 $< $@

$(DESTDIR)$(includedir)/$(package)/%.h: src/include/$(package)/%.h
	exec $(INSTALL) -D -m 644 $< $@

%.o: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) -c -o $@ $<

%.lo: %.c
	exec $(REALCC) $(CPPFLAGS_ALL) $(CFLAGS_ALL) $(CFLAGS_SHARED) -c -o $@ $<

lib%.a.xyzzy:
	exec $(AR) rc $@ $^
	exec $(RANLIB) $@

.PHONY: it all clean distclean tgz strip install install-lib install-include

.DELETE_ON_ERROR:
