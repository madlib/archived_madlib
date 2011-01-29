madlib_schema = SCHEMA_PLACEHOLDER
conf_defines = CONFDEFS
plpython_libdir = PLPYTHON_LIBDIR/madlib
dbapi = DBAPI2_PLACEHOLDER

%.sql: %.sql_in
	m4 -DMODULE_PATHNAME='$$libdir/madlib/$*' -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@
	
%.py: %.py_in
	m4 -DMODULE_PATHNAME='$$libdir/madlib/$*' -DMADLIB_SCHEMA=$(madlib_schema) -DDBAPI2=$(dbapi) $(conf_defines) $< >$@

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

# Put Python modules under $(plpython_libdir)
install-python:
ifneq (,$(PYTHON_built))
	if [ ! -d $(plpython_libdir) ]; then mkdir $(plpython_libdir); fi;
	if [ ! -f $(plpython_libdir)/__init__.py ]; then touch $(plpython_libdir)/__init__.py; fi; 
	@for file in $(PYTHON_built); do \
		echo "$(INSTALL_DATA) $$file '$(plpython_libdir)'"; \
		$(INSTALL_DATA) $$file '$(plpython_libdir)'; \
	done
endif # 

ifdef PYTHON_built
install: install-python
endif # PYTHON_built
