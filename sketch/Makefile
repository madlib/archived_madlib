MODULE_big = sketches
DATA_built = sketches.sql
PG_CPPFLAGS=-g
SRCS = fm.c sketch_support.c sortasort.c # countmin.c sketch_support_test.c
OBJS = $(SRCS:.c=.o)
SHLIB_LINK = -lpq
#DOCS = README.sketch_support

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
