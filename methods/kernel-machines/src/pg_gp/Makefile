online_sv.o : online_sv.c

MODULE_big = online_sv

DATA_built = online_sv.sql online_sv_drop.sql online_sv.py sql/online_sv_test.sql
PYTHON_built = online_sv.py
OBJS = online_sv.o

REGRESS = online_sv_test

PG_CONFIG = pg_config
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

test_output :
	psql -d regression_test -aqf sql/online_sv_test.sql_in &> expected/online_sv_test.out

include config.mk
