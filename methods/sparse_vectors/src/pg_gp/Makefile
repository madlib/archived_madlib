#Intel compiler
ifeq "$(CC)" "icc"
	USE_ICC = 1
endif

override CFLAGS+=-std=gnu99

SparseData.o : SparseData.c SparseData.h
sparse_vector.o : sparse_vector.c sparse_vector.h SparseData.h
operators.o : operators.c sparse_vector.h SparseData.h

MODULE_big = gp_svec
#Removed the installation SQL file from normal installation
#DATA = gp_svec.sql gp_sfv_test.sql gp_svec_test.sql

DATA_built = gp_svec.sql gp_svec_drop.sql
REGRESS = gp_svec_test
DATA = sql/gp_sfv_test.sql sql/gp_svec_test.sql
OBJS = gp_sfv.o sparse_vector.o operators.o SparseData.o

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

#CC=gcc -no-cpp-precomp -m64
#CC=gcc -no-cpp-precomp -m32
#CC=gcc -ftree-vectorizer-verbose=10 -ftree-vectorize
ifdef USE_ICC
	override CFLAGS=-O3 -Werror -std=c99 -vec-report2 -vec-threshold0
endif

test_output :
	psql -d regression_test -aqf sql/gp_svec_test.sql &> expected/gp_svec_test.out

include config.mk

