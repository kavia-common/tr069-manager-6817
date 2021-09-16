ROOT = ../../../..

include $(ROOT)/makefile.inc

COMPONENT = cwmp_plugin

# build destination directories
OBJDIR = $(ROOT)/output/$(MACHINE)/$(COMPONENT)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

TARGET_M4 = $(OBJDIR)/$(COMPONENT)-definition.odl

# directories
# source directories
SRCDIR = ./
INCDIR_PUB = /include
INCDIR_PRIV = $(ROOT)/include_priv
INCDIRS = $(INCDIR_PUB) $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)
OUTPUT_DIR = $(ROOT)/output
# files
HEADERS = $(wildcard $(INCDIR_PUB)/$(TARGET_NAME)/*.h)
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# compilation and linking flags
CFLAGS += -Wall -Wextra \
          -Wformat=2 -Wshadow -Wwrite-strings -Wredundant-decls \
          -Wmissing-declarations -Wno-attributes -Wno-format-nonliteral \
          -fPIC -g3 $(addprefix -I ,$(INCDIRS)) \
		  -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700

CFLAGS += -D_DEFAULT_SOURCE

ifeq ($(CC_NAME),g++)
    CFLAGS += -std=c++2a
else
	CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=c11
endif

LDFLAGS += $(STAGING_LIBDIR) -shared -fPIC
LDFLAGS += -lamxc -lamxd -lamxo -lamxb -lamxp -lsahtrace

# helper functions - used in multiple targets
define install_to
	$(INSTALL) -D -p -m 0644 $(TARGET_SO) $(1)/usr/lib/amx/$(COMPONENT)/$(COMPONENT).so
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT).odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT).odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)-defaults.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)-defaults.odl
	$(INSTALL) -D -p -m 0644 $(OBJDIR)/$(COMPONENT)-definition.odl $(1)/etc/amx/$(COMPONENT)/$(COMPONENT)-definition.odl
	$(INSTALL) -d -m 0755 $(1)$(BINDIR)
	$(INSTALL) -D -p -m 0755 scripts/$(COMPONENT).sh $(1)$(INITDIR)/$(COMPONENT)
endef

# targets
all: $(TARGET_SO) $(TARGET_A) $(TARGET_M4)

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so -o $@ $(OBJECTS) $(LDFLAGS)

$(TARGET_A): $(OBJECTS)
	$(AR) rcs $(@) $^

$(TARGET_M4):
	m4 odl/$(COMPONENT)-definition.odl.m4 > $(TARGET_M4)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf $(ROOT)/output/ $(ROOT)/$(COMPONENT)-*.* $(ROOT)/$(COMPONENT)_*.*

install: all
	$(call install_to,$(DEST))
	ln -sfr $(DEST)$(BINDIR)/amxrt $(DEST)$(BINDIR)/$(COMPONENT)

.PHONY: all clean
