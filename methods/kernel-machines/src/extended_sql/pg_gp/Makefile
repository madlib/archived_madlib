DATA_built = online_sv.sql.in
# DOCS = README

REGRESS = online_sv_test
DATA = sql/online_sv_test.sql

PG_CONFIG = pg_config
PGXS := $(shell pg_config --pgxs)
include $(PGXS)

include config.mk
