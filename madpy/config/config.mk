madlib_schema = SCHEMA_PLACEHOLDER
conf_defines = CONFDEFS

%.sql: %.sql.in
	m4 -DMODULE_PATHNAME='$$libdir/$*' -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@
	
%.py: %.py.in
	m4 -DMODULE_PATHNAME='$$libdir/$*' -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@

clean_data_built: 
	rm $(DATA_built)

install:
ifneq (,$(DATA_built))
	if [ ! -d $(datadir)/contrib/madlib ]; then mkdir $(datadir)/contrib/madlib; fi;
	@for file in $(DATA_built); do \
		echo "$(INSTALL_DATA) $$file '$(datadir)/contrib/madlib'"; \
		$(INSTALL_DATA) $$file '$(datadir)/contrib/madlib'; \
	done
endif # 

install-python:
ifneq (,$(PYTHON_built))
	if [ ! -d $(PYTHONPATH)/madlib ]; then mkdir $(PYTHONPATH)/madlib; fi;
	if [ ! -f $(PYTHONPATH)/madlib/__init__.py ]; then touch $(PYTHONPATH)/madlib/__init__.py; fi; 
	@for file in $(PYTHON_built); do \
		echo "$(INSTALL_DATA) $$file '$(PYTHONPATH)/madlib'"; \
		$(INSTALL_DATA) $$file '$(PYTHONPATH)/madlib'; \
	done
endif # 

ifdef PYTHON_built
install: install-python
endif # PYTHON_built
