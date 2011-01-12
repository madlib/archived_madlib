#BISON_FLAGS=-v --debug
#FLEX_FLAGS=-d
MATHJAX_DIR=doc/var/mathjax

doc/bin/sql_filter: doc/etc/sql.tab.c doc/etc/sql.yy.c Makefile
	gcc -o $@ $(filter %.c,$^)

%.tab.c %.tab.h: %.y
	# -d means: Write an extra output file containing macro definitions for the
	# token type names defined in the grammar and the semantic value type
	# YYSTYPE, as well as a few extern variable declarations
	cd $(dir $@) && bison ${BISON_FLAGS} -d $(notdir $<)

%.yy.c: %.l %.tab.h
	# -i means case-insensitive, -t means write to stdout
	flex ${FLEX_FLAGS} -i -t $< > $@

.INTERMEDIATE: doc/etc/sql.tab.c doc/etc/sql.tab.h doc/etc/sql.yy.c

devdoc: doc/bin/sql_filter
	doxygen doc/etc/developer.doxyfile

# Update the mathjax installation, which is used by oxygen
# See http://www.mathjax.org/resources/docs/?installation.html
mathjax:
	if [ -d ${MATHJAX_DIR} ]; then \
		cd ${MATHJAX_DIR}; \
		git pull origin; \
	else \
		git clone git://github.com/mathjax/MathJax.git ${MATHJAX_DIR}; \
		cd ${MATHJAX_DIR}; \
	fi; \
	if [ -f fonts.zip ]; then \
		if [ -d fonts ]; then echo rm -rf fonts; fi; \
		unzip fonts.zip; \
	fi
