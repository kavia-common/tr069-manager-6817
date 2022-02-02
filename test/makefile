
PKG_CONFIG_LIBDIR := /usr/lib/pkgconfig:/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:$(PKG_CONFIG_LIBDIR)
MACHINE = $(shell $(CC) -dumpmachine)
SUBDIRS := $(wildcard tr069-manager_*)
OBJDIR = ../output/$(MACHINE)/coverage
OUTPUT_DIR = ../output
COVERREPORT = report

run: $(OBJDIR)/
	$(MAKE) -C ../src/dmdeviceadapter/amx/adapter test
	@for dir in $(SUBDIRS); do make -C $$dir $@  || exit -1; done
	@rm -rf $(OBJDIR)/test_*

clean: 
	rm -rf $(OBJDIR)
	rm -rf $(OBJDIR)/$(COVERREPORT)
	find .. -name "run_test" -delete

# libdmda' objects files in this dir ../output/$(MACHINE)/libdmda_amx/
# and the other objects files of unit tests in OBJDIR
coverage: $(OBJDIR)/$(COVERREPORT)/
	@cd $(OUTPUT_DIR)/$(MACHINE) && \
	for i in $$(find . -type f -iname "*.o"); do \
		gcov -c -b -f --long-file-names --preserve-paths $$i > /dev/null; \
    done
	@cd $(OUTPUT_DIR)/$(MACHINE) && for i in $$(find . -name "*.h.gcov"); do rm $$i > /dev/null; done
	@mv $(OUTPUT_DIR)/$(MACHINE)/*.gcov $(OBJDIR)
	cd $(OBJDIR) && gcovr -k -p -r ../../.. -s -g --html --html-details -o ./$(COVERREPORT)/index.html
	cd $(OBJDIR) && gcovr -k -p -r ../../.. -s -g | tee ./$(COVERREPORT)/gcovr_summary.txt

$(OBJDIR)/:
	mkdir -p $@

$(OBJDIR)/$(COVERREPORT)/:
	mkdir -p $@

.PHONY: run clean coverage

