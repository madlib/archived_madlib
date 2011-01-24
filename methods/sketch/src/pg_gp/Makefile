PG_CONFIG = pg_config
MODULE_big = sketches
DATA_built = sketches.sql sketches_drop.sql
SRCS = fm.c sketch_support.c sortasort.c countmin.c mfvsketch.c
OBJS = $(SRCS:.c=.o)
SHLIB_LINK = -lpq
REGRESS = setup fm_test cm_test sketch_support_test mfv_test
#DOCS = README.sketch_support

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include config.mk
