DATA_built = regression.sql logRegress.py regression_drop.sql
PYTHON_built = logRegress.py

#SRCS = $(wildcard *.c)
SOURCES = $(wildcard *.c)

REGRESS = multLinRegression_test

# commenting the next line out means that, for the regression tests, the shared
# library will be looked for in the PostgreSQL library directory and not in
# the current directory
REGRESS_OPTS := --dlpath=.

# MODULE_big should have the same name as the fw SQL file specified in
# Install.yml
MODULE_big = regression
OBJS = $(SOURCES:%.c=%.o)

# We want to include debugging symbols.
# For release versions, comment out the next line.
COPT = -g3 -O0 -std=c99

# If we wanted to ensure thread-safety, we need to define _REENTRANT, so that
# the appropriate thread-safe functions are used (such as lgamma_r). Then,
# since they are non-standard (but widely available), we need gnu99 instead of c99.
# COPT += -D_REENTRANT -std=gnu99

# We want to link to the LAPACK library.
SHLIB_LINK += -llapack

PG_CONFIG = pg_config 
PGXS := $(shell $(PG_CONFIG) --pgxs) 

include $(PGXS)
include config.mk 

# do not delete .o files because we want them for debugging
.PRECIOUS: %.o
