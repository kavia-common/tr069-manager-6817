include ../test_defines.mk

INCDIR += $(realpath /include)
PLUGIN_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/cwmp_plugin)
SOURCES += $(wildcard $(PLUGIN_SRCDIR)/*.c)
TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o))) 
LDFLAGS += -lsahtrace -luriparser -lfiletransfer -lgmap-client

WRAP_FUNC = -Wl,--wrap=

MOCK_WRAP += gmap_query_open \
			 gmap_query_close

LDFLAGS += $(addprefix $(WRAP_FUNC),$(MOCK_WRAP))

include ../test_targets.mk
