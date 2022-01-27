include ../../../makefile.inc

COMPONENT = libhttpparser
# build destination directories
OBJDIR = ../../../output/$(MACHINE)/$(COMPONENT)


# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV) $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) \
				 $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

OUTPUT_DIR = ../../../output
# files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

CFLAGS += -Wall -Wextra -Werror -Wno-strict-aliasing \
		  -fPIC -g -Os -I../../include_priv -I$(STAGINGDIR)/include \
		  -D_GNU_SOURCE -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL_DEFAULT=700

LDFLAGS += $(STAGING_LIBDIR) -ldl -lsahtrace

all: bannerCompile $(TARGET_SO)

bannerCompile:
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
