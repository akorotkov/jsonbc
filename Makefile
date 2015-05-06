MODULE_big = jsonb
OBJS = jsonb.o jsonb_gin.o jsonb_op.o jsonb_util.o json.o numeric.o jsonfuncs.o
EXTENSION = jsonb
DATA = jsonb--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
