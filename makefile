include makefile.inc

NOW = $(shell date +"%Y-%m-%d(%H:%M:%S %z)")

# Extra destination directories
PKGDIR = ./output/$(MACHINE)/pkg/

# helper functions - used in multiple targets
define install_to
	$(INSTALL) -D -p -m 0644 output/$(MACHINE)/cwmp_plugin/cwmp_plugin.so $(1)/usr/lib/amx/cwmp_plugin/cwmp_plugin.so
	$(INSTALL) -d -m 0755 $(1)$(BINDIR)
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin.odl $(1)/etc/amx/cwmp_plugin/cwmp_plugin.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-defaults.odl $(1)/etc/amx/cwmp_plugin/cwmp_plugin-defaults.odl
	$(INSTALL) -D -p -m 0644 output/$(MACHINE)/cwmp_plugin/cwmp_plugin-definition.odl $(1)/etc/amx/cwmp_plugin/cwmp_plugin-definition.odl
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/cwmp_plugin.sh $(1)$(INITDIR)/cwmp_plugin
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libhttpparser/libhttpparser.so $(1)$(LIBDIR)/libhttpparser.so
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libdmda_amx/libdmda_amx.so $(1)$(LIBDIR)/libdmda_amx.so
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/cwmpd/cwmpd $(1)$(BINDIR)/cwmpd
endef

define create_changelog
	@$(ECHO) "Update changelog"
	mv CHANGELOG.md CHANGELOG.md.bak
	head -n 9 CHANGELOG.md.bak > CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(ECHO) "## Release $(VERSION) - $(NOW)" >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(GIT) log --pretty=format:"- %s" $$($(GIT) describe --tags | grep -v "merge" | cut -d'-' -f1)..HEAD  >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	tail -n +10 CHANGELOG.md.bak >> CHANGELOG.md
	rm CHANGELOG.md.bak
endef

# targets
all:
	$(MAKE) -C src/dmdeviceadapter/amx all
	$(MAKE) -C libs/src/httpparser all
	$(MAKE) -C src/dmmain all

clean:
	$(MAKE) -C src/dmdeviceadapter/amx clean
	$(MAKE) -C libs/src/httpparser clean
	$(MAKE) -C src/dmmain clean
	$(MAKE) -C test clean

install: all
	$(call install_to,$(DEST))
	ln -sfr $(DEST)$(BINDIR)/amxrt $(DEST)$(BINDIR)/cwmp_plugin

package: all
	$(call install_to,$(PKGDIR))
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

test:
	$(MAKE) -C src/dmdeviceadapter/amx/adapter test
	$(MAKE) -C test run
	$(MAKE) -C test coverage

.PHONY: all clean changelog install package test