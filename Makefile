MODULE_big = jsonbc
OBJS = jsonbc.o jsonbc_gin.o jsonbc_op.o jsonbc_util.o dict.o jsonfuncs.o
EXTENSION = jsonbc
DATA = jsonbc--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
