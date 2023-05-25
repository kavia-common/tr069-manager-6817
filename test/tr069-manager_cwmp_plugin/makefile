include ../test_defines.mk

INCDIR += $(realpath /include)
PLUGIN_SRCDIR = $(realpath ../../src/dmdeviceadapter/amx/cwmp_plugin)
SOURCES += $(wildcard $(PLUGIN_SRCDIR)/*.c)
TARGET = run_test
SOURCES += $(wildcard ./*.c)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))
LDFLAGS += -Wl,--wrap=start_cwmpd \
		   -Wl,--wrap=stop_cwmpd \
		   -Wl,--wrap=netmodel_cleanup \
		   -Wl,--wrap=netmodel_initialize \
		   -Wl,--wrap=netmodel_openQuery_luckyAddrAddress \
		   -Wl,--wrap=netmodel_closeQuery \
		   -Wl,--wrap=ipv4address_changed_cb \
		   -Wl,--wrap=cwmp_plugin_transfer_init \
		   -lsahtrace -luriparser -lfiletransfer -ldhcpoptions


include ../test_targets.mk