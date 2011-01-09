DATA_built = kmeans_create.sql kmeans_drop.sql kmeans.py
PYTHON_built = kmeans.py
REGRESS = setup test cleanup      
      
PGXS := $(shell pg_config --pgxs) 
include $(PGXS) 
include config.mk 
