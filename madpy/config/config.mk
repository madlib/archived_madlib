madlib_schema = SCHEMA_PLACEHOLDER
conf_defines = CONFDEFS

%.sql: %.sql.in
	m4 -DMODULE_PATHNAME='$$libdir/$*' -DMADLIB_SCHEMA=$(madlib_schema) -DMADLIB_SCHEMA=$(madlib_schema) $(conf_defines) $< >$@
	
clean_data_built: 
	rm $(DATA_built)

