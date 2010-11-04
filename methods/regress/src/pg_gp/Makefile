CEPHES = $(wildcard cephes/*.c)
OBJ_CEPHES = $(CEPHES:%.c=%.o)
REGRESS = multLinRegression_test

# commenting the next line out means that, for the regression tests, the shared
# library will be looked for in the PostgreSQL library directory and not in
# the current directory
REGRESS_OPTS := --dlpath=.

# Changing the module name unfortunately also requires changing
# input/createFuncsAndData.source
MODULE_big = multLinRegression
OBJS = regress.o pinv.o matrix.o $(OBJ_CEPHES)

# We want to include debugging symbols.
# For release versions, comment out the next line.
COPT = -g3 -O0

# We want to link to the LAPACK library.
SHLIB_LINK += -llapack

PG_CONFIG = pg_config 
PGXS := $(shell $(PG_CONFIG) --pgxs) 

include $(PGXS)

# do not delete .o files because we want them for debugging
.PRECIOUS: %.o
