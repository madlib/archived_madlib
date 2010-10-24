PG_CONFIG = pg_config
MODULE_big = sketches
DATA_built = sketches.sql
PG_CPPFLAGS=-g
SRCS = fm.c sketch_support.c sortasort.c countmin.c
OBJS = $(SRCS:.c=.o)
SHLIB_LINK = -lpq
REGRESS = fm_test cm_test sketch_support_test
#DOCS = README.sketch_support

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
