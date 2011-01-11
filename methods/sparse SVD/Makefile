DATA_built = svd_sparse_create.sql svd_sparse_drop.sql svd_sparse.py test.sql
PYTHON_built = svd_sparse.py
REGRESS = setup test cleanup
      
PGXS := $(shell pg_config --pgxs)
include $(PGXS)
include config.mk 