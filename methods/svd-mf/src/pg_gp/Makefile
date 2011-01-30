DATA_built = svdmf.sql svdmf_drop.sql svdmf.py sql/cleanup.sql sql/test.sql
PYTHON_built = svdmf.py
REGRESS = setup test cleanup

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include config.mk
