DATA_built = online_sv.sql online_sv_drop.sql
# DOCS = README

REGRESS = online_sv_test
DATA = sql/online_sv_test.sql

PG_CONFIG = pg_config
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

include config.mk
