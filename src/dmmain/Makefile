include ../../makefile.inc

#lib NAME
COMPONENT = cwmpd
OUTPUT_DIR = ../../output
# build destination directories
OBJDIR = $(OUTPUT_DIR)/$(MACHINE)/$(COMPONENT)

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../../include_priv
ifdef STAGINGDIR
#libwebsocket default install (/usr/local/include,/usr/local/lib)
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), -I$(STAGINGDIR)/include) \
		  $(if $(STAGINGDIR), -I$(STAGINGDIR)/usr/include) \
		  $(if $(STAGINGDIR), -I$(STAGINGDIR)/usr/local/include)

STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) \
				 $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib) \
				 $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/local/lib)
else
#staging dir is not defined ( on docker we need this otherwise build will fail)
INCDIRS = $(INCDIR_PRIV) -I/usr/include -I/usr/local/include
STAGING_LIBDIR =-L/usr/local/lib/ -L/usr/lib -L/lib -L/usr/lib/$(MACHINE)/
endif


# files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# TARGETS
TARGET_BIN = $(OBJDIR)/$(COMPONENT)

CFLAGS += -Werror -Wall -Wextra -Werror\
          -Wformat=2 -Wshadow \
          -Wwrite-strings -Wredundant-decls \
          -Wno-format-nonliteral \
		  -Wformat-security \
		  -I$(INCDIRS) -fPIC -g3 \
		  -I$(STAGINGDIR)/include -D_GNU_SOURCE \
		  $(shell pkg-config --cflags libxml-2.0) \
		  -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700

ifeq ($(CC_NAME),g++)
	CFLAGS += -std=c++2a
else
	CFLAGS += -std=gnu11
endif

LDFLAGS += -ldl $(STAGING_LIBDIR) -lwebsockets \
		  $(shell pkg-config --libs libevent) \
		  -lamxc -lamxd -lamxo -lamxb -lamxp -lamxa -lsahtrace \
		  -ldmcom -ldmcommon -ldmengine -ldmxml_ixml -lcares -lxml2
# helper functions - used in multiple targets
define install_to
	$(INSTALL) -d $(1)/usr/bin/
	$(INSTALL) -m 0755 $(OBJDIR)/$(COMPONENT) $(1)/usr/bin/
endef

LDFLAGS_PRIO = $(if $(STAGINGDIR), -L$(STAGINGDIR)/opt/prplos/lib) \
			   $(if $(STAGINGDIR), -L$(STAGINGDIR)/opt/prplos/usr/lib) \
			   $(if $(STAGINGDIR), -L$(STAGINGDIR)/opt/prplos/usr/local/lib)

CFLAGS_PRIO = $(if $(STAGINGDIR), -I$(STAGINGDIR)/opt/prplos/include) \
			  $(if $(STAGINGDIR), -I$(STAGINGDIR)/opt/prplos/usr/include) \
			  $(if $(STAGINGDIR), -I$(STAGINGDIR)/opt/prplos/usr/local/include)

all: bannerCompile $(TARGET_BIN)

bannerCompile:
	@echo "*********************** Building $(COMPONENT) *********************************"

$(TARGET_BIN): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS_PRIO) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS_PRIO) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS_PRIO) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf ../../output/ ../../$(COMPONENT)-*.* ../../$(COMPONENT)_*.*

.PHONY: all clean
