include makefile.inc

NOW = $(shell date +"%Y-%m-%d(%H:%M:%S %z)")

# Extra destination directories
PKGDIR = ./output/$(MACHINE)/pkg/

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
	$(MAKE) -C src/dmmain all
	$(MAKE) -C src/dmdeviceadapter/amx/cwmp_plugin/scripts all

clean:
	$(MAKE) -C src/dmdeviceadapter/amx clean
	$(MAKE) -C src/dmmain clean
	$(MAKE) -C src/dmdeviceadapter/amx/cwmp_plugin/scripts clean
	$(MAKE) -C test clean

install: all
	$(INSTALL) -D -p -m 0644 output/$(MACHINE)/cwmp_plugin/cwmp_plugin.so $(DEST)/usr/lib/amx/cwmp_plugin/cwmp_plugin.so
	$(INSTALL) -d -m 0755 $(DEST)$(BINDIR)
	ln -nsfr $(DEST)$(BINDIR)/amxrt $(DEST)$(BINDIR)/cwmp_plugin
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin.odl $(DEST)/etc/amx/cwmp_plugin/cwmp_plugin.odl
	$(INSTALL) -D -p -m 0644 odl/amx-processmonitor/test-cwmp_plugin.odl $(DEST)/etc/amx/amx-processmonitor/defaults.d/43-test-cwmp_plugin.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/defaults.d/cwmp_plugin-defaults.odl $(DEST)/etc/amx/cwmp_plugin/defaults.d/00_cwmp_plugin-defaults.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl $(DEST)/etc/amx/cwmp_plugin/cwmp_plugin-definition.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-prefix.odl $(DEST)/etc/amx/cwmp_plugin/cwmp_plugin-prefix.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmpd-config.odl $(DEST)/etc/amx/cwmp_plugin/cwmpd-config.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-mapping.odl $(DEST)/etc/amx/tr181-device/extensions/01_cwmp_plugin_mapping.odl
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/cwmp_plugin.sh $(DEST)$(INITDIR)/cwmp_plugin
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_1_fw_upgrade.sh $(DEST)$(BINDIR)/tr069_1_fw_upgrade
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_2_vendor_logs_prepare.sh $(DEST)$(BINDIR)/tr069_2_vendor_logs_prepare
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_1_vendor_conf_prepare.sh $(DEST)$(BINDIR)/tr069_1_vendor_conf_prepare
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_2_webcontent.sh $(DEST)$(BINDIR)/tr069_2_webcontent
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_3_conf_apply.sh $(DEST)$(BINDIR)/tr069_3_conf_apply
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libdmda_amx/libdmda_amx.so $(DEST)$(LIBDIR)/libdmda_amx.so
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/cwmpd/cwmpd $(DEST)$(BINDIR)/cwmpd
	$(INSTALL) -D -p -m 0660 acl/cwmp/cwmp_plugin.json $(DEST)$(ACLDIR)/cwmp/cwmp_plugin.json

package: all
	$(INSTALL) -D -p -m 0644 output/$(MACHINE)/cwmp_plugin/cwmp_plugin.so $(PKGDIR)/usr/lib/amx/cwmp_plugin/cwmp_plugin.so
	$(INSTALL) -d -m 0755 $(PKGDIR)$(BINDIR)
	rm -f $(PKGDIR)$(BINDIR)/cwmp_plugin
	ln -nsfr $(PKGDIR)$(BINDIR)/amxrt $(PKGDIR)$(BINDIR)/cwmp_plugin
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin.odl $(PKGDIR)/etc/amx/cwmp_plugin/cwmp_plugin.odl
	$(INSTALL) -D -p -m 0644 odl/amx-processmonitor/test-cwmp_plugin.odl $(PKGDIR)/etc/amx/amx-processmonitor/defaults.d/43-test-cwmp_plugin.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/defaults.d/cwmp_plugin-defaults.odl $(PKGDIR)/etc/amx/cwmp_plugin/defaults.d/00_cwmp_plugin-defaults.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-definition.odl $(PKGDIR)/etc/amx/cwmp_plugin/cwmp_plugin-definition.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-prefix.odl $(PKGDIR)/etc/amx/cwmp_plugin/cwmp_plugin-prefix.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmpd-config.odl $(PKGDIR)/etc/amx/cwmp_plugin/cwmpd-config.odl
	$(INSTALL) -D -p -m 0644 src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin-mapping.odl $(PKGDIR)/etc/amx/tr181-device/extensions/01_cwmp_plugin_mapping.odl
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/cwmp_plugin.sh $(PKGDIR)$(INITDIR)/cwmp_plugin
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_1_fw_upgrade.sh $(PKGDIR)$(BINDIR)/tr069_1_fw_upgrade
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_2_vendor_logs_prepare.sh $(PKGDIR)$(BINDIR)/tr069_2_vendor_logs_prepare
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_1_vendor_conf_prepare.sh $(PKGDIR)$(BINDIR)/tr069_1_vendor_conf_prepare
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_2_webcontent.sh $(PKGDIR)$(BINDIR)/tr069_2_webcontent
	$(INSTALL) -D -p -m 0755 src/dmdeviceadapter/amx/cwmp_plugin/scripts/tr069_3_conf_apply.sh $(PKGDIR)$(BINDIR)/tr069_3_conf_apply
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/libdmda_amx/libdmda_amx.so $(PKGDIR)$(LIBDIR)/libdmda_amx.so
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/cwmpd/cwmpd $(PKGDIR)$(BINDIR)/cwmpd
	$(INSTALL) -D -p -m 0660 acl/cwmp/cwmp_plugin.json $(PKGDIR)$(ACLDIR)/cwmp/cwmp_plugin.json
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

doc:
	$(eval ODLFILES += src/dmdeviceadapter/amx/cwmp_plugin/odl/cwmp_plugin.odl)

	mkdir -p output/xml
	mkdir -p output/html
	mkdir -p output/confluence
	amxo-cg -Gxml,output/xml/$(COMPONENT).xml $(or $(ODLFILES), "")
	amxo-xml-to -x html -o output-dir=output/html -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml
	amxo-xml-to -x confluence -o output-dir=output/confluence -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml

test:
	$(MAKE) -C test run
	$(MAKE) -C test coverage

.PHONY: all clean changelog install package doc test