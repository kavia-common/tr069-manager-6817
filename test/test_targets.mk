all: $(TARGET)

run: $(TARGET)
	set -o pipefail; valgrind --error-exitcode=1 ./$< 2>&1 | tee -a $(OBJDIR)/unit_test_results.txt;

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) -fprofile-arcs -ftest-coverage

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: ./%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -fprofile-arcs -ftest-coverage -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)
	
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -fprofile-arcs -ftest-coverage -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

ifdef MOCK_SRCDIR
$(OBJDIR)/%.o: $(MOCK_SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -fprofile-arcs -ftest-coverage -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)
endif

ifdef PLUGIN_SRCDIR
$(OBJDIR)/%.o: $(PLUGIN_SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -fprofile-arcs -ftest-coverage -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)
endif

$(OBJDIR)/:
	mkdir -p $@

clean:
	rm -rf $(TARGET) $(OBJDIR)

.PHONY: clean $(OBJDIR)/
