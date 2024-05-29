include ../test_defines.mk

INCDIR += $(realpath /include)
PLUGIN_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/adapter)
SOURCES += $(wildcard $(PLUGIN_SRCDIR)/*.c)

TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
LDFLAGS += -lamxa -lamxo -ldmengine -ldmcommon -ldmcom -ldmxml_ixml -lsahtrace -ldmda_amx\
			-Wl,--wrap=DM_ENG_InformMessageScheduler_SendDUStateChangeComplete\
			-Wl,--wrap=DM_ENG_NotificationInterface_timerStart

include ../test_targets.mk