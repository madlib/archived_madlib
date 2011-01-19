DATA_built = kmeans_create.sql kmeans_drop.sql kmeans.py sql/setup.sql sql/test.sql 
PYTHON_built = kmeans.py
REGRESS = setup test cleanup      
      
PGXS := $(shell pg_config --pgxs) 
include $(PGXS) 
include config.mk 
