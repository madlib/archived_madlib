madlib_schema = SCHEMA_PLACEHOLDER
conf_defines = CONFDEFS
python_target = PYTHON_TARGET_DIR

%.sql: %.sql_in
	m4 -DMODULE_PATHNAME='$$libdir/madlib/$*' -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@
	
%.py: %.py_in
	m4 -DMODULE_PATHNAME='$$libdir/madlib/$*' -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@

clean_data_built: 
	rm $(DATA_built)

# Commented out the DATA_built redirection for now
#install:
#ifneq (,$(DATA_built))
#	if [ ! -d $(datadir)/contrib/madlib ]; then mkdir $(datadir)/contrib/madlib; fi;
#	@for file in $(DATA_built); do \
#		echo "$(INSTALL_DATA) $$file '$(datadir)/contrib/madlib'"; \
#		$(INSTALL_DATA) $$file '$(datadir)/contrib/madlib'; \
#	done
#endif # 

# Put C shared objects under $(pkglibdir)/madlib
override pkglibdir := $(pkglibdir)/madlib

# Put Python modules under $(python_target)
install-python:
ifneq (,$(PYTHON_built))
	if [ ! -d $(python_target) ]; then mkdir $(python_target); fi;
	if [ ! -f $(python_target)/__init__.py ]; then touch $(python_target)/__init__.py; fi; 
	@for file in $(PYTHON_built); do \
		echo "$(INSTALL_DATA) $$file '$(python_target)'"; \
		$(INSTALL_DATA) $$file '$(python_target)'; \
	done
endif # 

ifdef PYTHON_built
install: install-python
endif # PYTHON_built
