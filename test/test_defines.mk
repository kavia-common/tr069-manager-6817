MACHINE = $(shell $(CC) -dumpmachine)

SRCDIR = $(realpath ../../src)
OBJDIR = $(realpath ../../output/$(MACHINE)/coverage)
INCDIR = $(realpath ../../include ../../include_priv ../../libs/include_priv)

HEADERS = $(wildcard $(INCDIR)/*.h)
SOURCES = $(wildcard $(SRCDIR)/*.c) 

CFLAGS += -Wall -Wextra -Wno-attributes --std=gnu99 -g3 \
		  $(addprefix -I ,$(INCDIR)) -I$(OBJDIR)/.. \
		  -fkeep-inline-functions -fkeep-static-functions \
		  -Wno-format-nonliteral \
		  $(shell pkg-config --cflags cmocka) -pthread -DUNIT_TEST

LDFLAGS += -fkeep-inline-functions -fkeep-static-functions \
		   $(shell pkg-config --libs cmocka) \
		   -lamxc -lamxp -lamxd -lamxo -lamxb -lamxm\
		   -ldl -lpthread -lsahtrace -lnetmodel -lamxut
