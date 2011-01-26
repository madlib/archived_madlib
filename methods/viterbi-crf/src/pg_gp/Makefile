PG_CONFIG = pg_config
MODULE_big = viterbi
DATA_built = viterbi_top1.sql viterbi.sql import_data.sql
SRCS = topk_array.c
OBJS = $(SRCS:.c=.o)
SHLIB_LINK = -lpq
#REGRESS = setup fm_test cm_test sketch_support_test mfv_test
#DOCS = README.sketch_support

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include config.mk
