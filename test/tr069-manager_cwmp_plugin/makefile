include ../test_defines.mk

INCDIR += $(realpath /include)
PLUGIN_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/cwmp_plugin)
SOURCES += $(wildcard $(PLUGIN_SRCDIR)/*.c)
TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
LDFLAGS += -Wl,--wrap=start_cwmpd -Wl,--wrap=stop_cwmpd -lsahtrace -luriparser


include ../test_targets.mk