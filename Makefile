#BISON_FLAGS=-v --debug
#FLEX_FLAGS=-d

doc/bin/sql_filter: doc/etc/sql.tab.c doc/etc/sql.yy.c Makefile
	gcc -o $@ -lfl $(filter %.c,$^)

%.tab.c %.tab.h: %.y
	cd $(dir $@) && bison ${BISON_FLAGS} -d $(notdir $<)

%.yy.c: %.l %.tab.h
	flex ${FLEX_FLAGS} --case-insensitive --stdout $< > $@

.INTERMEDIATE: doc/etc/sql.tab.c doc/etc/sql.tab.h doc/etc/sql.yy.c

devdoc: doc/bin/sql_filter
	doxygen doc/etc/developer.doxyfile
