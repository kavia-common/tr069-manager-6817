include ../../makefile.inc

#lib NAME
COMPONENT = cwmpd
OUTPUT_DIR = ../../output
# build destination directories
OBJDIR = $(OUTPUT_DIR)/$(MACHINE)/$(COMPONENT)

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../../include_priv -I../../libs/include_priv
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
STAGING_LIBDIR =-L/usr/local/lib/ -L/usr/lib -L/lib -L/usr/lib/$(MACHINE)/ -L$(OUTPUT_DIR)/$(MACHINE)/libhttpparser/
endif


# files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# TARGETS
TARGET_BIN = $(OBJDIR)/$(COMPONENT)
              #-DSAHTRACES_ENABLED  -Wall -Wextra -Werror
CFLAGS += -I$(INCDIRS) -fPIC -g -Os \
		  -I$(STAGINGDIR)/include -D_GNU_SOURCE \
		  -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700

ifeq ($(CC_NAME),g++)
	CFLAGS += -std=c++2a
else
	CFLAGS += -std=c11
endif

LDFLAGS += $(STAGING_LIBDIR) -lwebsockets -levent -ldl \
		  -lamxc -lamxd -lamxo -lamxb -lamxp -lsahtrace \
		  -ldmcom -ldmcommon -ldmengine -ldmxml_ixml -lhttpparser
# helper functions - used in multiple targets
define install_to
	$(INSTALL) -d $(1)/usr/bin/
	$(INSTALL) -m 0755 $(OBJDIR)/$(COMPONENT) $(1)/usr/bin/
endef

all: bannerCompile $(TARGET_BIN)

bannerCompile:
	@echo "*********************** Building $(COMPONENT) *********************************"

bannerInstall:
	@echo "*********************** Istalling $(COMPONENT) ********************************"

$(TARGET_BIN): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf ../../output/ ../../$(COMPONENT)-*.* ../../$(COMPONENT)_*.*

install: all bannerInstall
	$(call install_to,$(DEST))

.PHONY: all clean install release
