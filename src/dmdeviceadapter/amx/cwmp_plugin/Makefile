ROOT = ../../../..

include $(ROOT)/makefile.inc

COMPONENT = cwmp_plugin

# build destination directories
OBJDIR = $(ROOT)/output/$(MACHINE)/$(COMPONENT)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

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
CFLAGS += -Wall -Wextra -Werror \
          -Wformat=2 -Wshadow -Wwrite-strings -Wredundant-decls \
          -Wmissing-declarations -Wno-attributes -Wno-format-nonliteral \
          -fPIC -g3 $(addprefix -I ,$(INCDIRS)) \
		  -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700

CFLAGS += -D_DEFAULT_SOURCE

ifeq ($(CC_NAME),g++)
    CFLAGS += -std=c++2a
else
	CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=gnu11
endif

LDFLAGS += $(STAGING_LIBDIR) -shared -fPIC
LDFLAGS += -lamxc -lamxd -lamxo -lamxb -lamxp -lamxm -lamxa -lsahtrace -lnetmodel -lfiletransfer -lgmap-client

CFLAGS:=$(filter-out -Os -g3 -g,$(CFLAGS))
CFLAGS += -ggdb3

# targets
all: banner $(TARGET_SO) $(TARGET_A)

banner:
	@echo "*********************** Building $(COMPONENT).so *********************************"

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so -o $@ $(OBJECTS) $(LDFLAGS)

$(TARGET_A): $(OBJECTS)
	$(AR) rcs $(@) $^

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf $(ROOT)/output/ $(ROOT)/$(COMPONENT)-*.* $(ROOT)/$(COMPONENT)_*.*

.PHONY: all clean
