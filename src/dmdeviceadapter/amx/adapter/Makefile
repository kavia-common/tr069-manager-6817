
ROOT = ../../../..

include $(ROOT)/makefile.inc

#lib NAME
COMPONENT = libdmda_amx
OUTPUT_DIR = $(ROOT)/output
# build destination directories
OBJDIR = $(ROOT)/output/$(MACHINE)/$(COMPONENT)

SRCDIR = .
INCDIR_PRIV = $(ROOT)/include_priv -I$(ROOT)/libs/include_priv
# files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) \
				 $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib) \
				 $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/local/lib)

CFLAGS += -D_GNU_SOURCE -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700 \
		  -trigraphs -fPIC -g -Wall -Wextra -Werror \
		  -I$(INCDIR_PRIV) -I$(ROOT)/include -I$(STAGINGDIR)/include \
		  -I$(STAGINGDIR)/usr/include

ifeq ($(CC_NAME),g++)
	CFLAGS += -std=c++2a
else
	CFLAGS += -std=c11
endif

LDFLAGS += $(STAGING_LIBDIR) -ldl -lsahtrace \
		  -lamxc -lamxd -lamxo -lamxb -lamxp \
		  -ldmengine -ldmcommon -ldmcom -ldmxml_ixml

ifeq ($(CONFIG_TR69_TEST_COVERAGE),y)
  CFLAGS += -fprofile-arcs -ftest-coverage
  LDFLAGS += -fprofile-arcs
endif

test: all

all: banner $(TARGET_SO)

banner:
	@echo "*********************** Building $(COMPONENT).so *********************************"

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so -shared -o $@ $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf ../../../output/ ../../../$(COMPONENT)-*.* ../../../$(COMPONENT)_*.*

.PHONY: all clean 
