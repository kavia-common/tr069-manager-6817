include ../test_defines.mk

INCDIR += $(realpath /include)
PLUGIN_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/cwmp_plugin)
DMMAIN_SRCDIR = $(realpath ../../src/dmmain)
SOURCES += $(wildcard $(PLUGIN_SRCDIR)/*.c) $(DMMAIN_SRCDIR)/cwmpd_get_certificate_uri.c
TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
LDFLAGS += -Wl,--wrap=amxb_call \
		   -Wl,--wrap=amxb_be_who_has \
		   -lsahtrace -luriparser -lfiletransfer -lgmap-client \
		   $(shell pkg-config --libs libevent)

include ../test_targets.mk
