include ../test_defines.mk

INCDIR += $(realpath ../common)
MOCK_SRCDIR = $(realpath ../common)
SOURCES += $(wildcard $(MOCK_SRCDIR)/*.c)

ADAPTER_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/adapter)
SOURCES += $(wildcard $(ADAPTER_SRCDIR)/*.c)

TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
LDFLAGS += -lamxo -ldmengine -ldmcommon -ldmcom -ldmxml_ixml -lsahtrace

include ../test_targets.mk
