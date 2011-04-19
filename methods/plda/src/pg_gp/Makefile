override CFLAGS+=-std=gnu99

SOURCES = plda_support.c
OBJS = $(SOURCES:%.c=%.o)

MODULE_big = plda_support
DATA_built = plda.sql plda_drop.sql sql/plda_test.sql sql/importTestcases.sql sql/importSmall.sql plda.py

PYTHON_built = plda.py

REGRESS = plda_test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)

include $(PGXS)

generateTestCases : generateTestCases.cc
	g++ -Wall -O2 generateTestCases.cc -o generateTestCases

include config.mk
