/*-------------------------------------------------------------------------
 *
 * jsonbc_op.c
 *	 Special operators for jsonbc only, used by various index access methods
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonbc_op.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "utils/jsonbc.h"

Datum
jsonbc_exists(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	text	   *key = PG_GETARG_TEXT_PP(1);
	JsonbcValue	kval;
	JsonbcValue *v = NULL;

	/*
	 * We only match Object keys (which are naturally always Strings), or
	 * string elements in arrays.  In particular, we do not match non-string
	 * scalar elements.  Existence of a key/element is only considered at the
	 * top level.  No recursion occurs.
	 */
	kval.type = jbvString;
	kval.val.string.val = VARDATA_ANY(key);
	kval.val.string.len = VARSIZE_ANY_EXHDR(key);

	v = findJsonbcValueFromContainer(&jb->root,
									JB_FOBJECT | JB_FARRAY,
									&kval);

	PG_RETURN_BOOL(v != NULL);
}

Datum
jsonbc_exists_any(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	int			i;
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			elem_count;

	deconstruct_array(keys, TEXTOID, -1, false, 'i', &key_datums, &key_nulls,
					  &elem_count);

	for (i = 0; i < elem_count; i++)
	{
		JsonbcValue	strVal;

		if (key_nulls[i])
			continue;

		strVal.type = jbvString;
		strVal.val.string.val = VARDATA(key_datums[i]);
		strVal.val.string.len = VARSIZE(key_datums[i]) - VARHDRSZ;

		if (findJsonbcValueFromContainer(&jb->root,
										JB_FOBJECT | JB_FARRAY,
										&strVal) != NULL)
			PG_RETURN_BOOL(true);
	}

	PG_RETURN_BOOL(false);
}

Datum
jsonbc_exists_all(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	ArrayType  *keys = PG_GETARG_ARRAYTYPE_P(1);
	int			i;
	Datum	   *key_datums;
	bool	   *key_nulls;
	int			elem_count;

	deconstruct_array(keys, TEXTOID, -1, false, 'i', &key_datums, &key_nulls,
					  &elem_count);

	for (i = 0; i < elem_count; i++)
	{
		JsonbcValue	strVal;

		if (key_nulls[i])
			continue;

		strVal.type = jbvString;
		strVal.val.string.val = VARDATA(key_datums[i]);
		strVal.val.string.len = VARSIZE(key_datums[i]) - VARHDRSZ;

		if (findJsonbcValueFromContainer(&jb->root,
										JB_FOBJECT | JB_FARRAY,
										&strVal) == NULL)
			PG_RETURN_BOOL(false);
	}

	PG_RETURN_BOOL(true);
}

Datum
jsonbc_contains(PG_FUNCTION_ARGS)
{
	Jsonbc	   *val = PG_GETARG_JSONB(0);
	Jsonbc	   *tmpl = PG_GETARG_JSONB(1);

	JsonbcIterator *it1,
			   *it2;

	if (JB_ROOT_IS_OBJECT(val) != JB_ROOT_IS_OBJECT(tmpl))
		PG_RETURN_BOOL(false);

	it1 = JsonbcIteratorInit(&val->root);
	it2 = JsonbcIteratorInit(&tmpl->root);

	PG_RETURN_BOOL(JsonbcDeepContains(&it1, &it2));
}

Datum
jsonbc_contained(PG_FUNCTION_ARGS)
{
	/* Commutator of "contains" */
	Jsonbc	   *tmpl = PG_GETARG_JSONB(0);
	Jsonbc	   *val = PG_GETARG_JSONB(1);

	JsonbcIterator *it1,
			   *it2;

	if (JB_ROOT_IS_OBJECT(val) != JB_ROOT_IS_OBJECT(tmpl))
		PG_RETURN_BOOL(false);

	it1 = JsonbcIteratorInit(&val->root);
	it2 = JsonbcIteratorInit(&tmpl->root);

	PG_RETURN_BOOL(JsonbcDeepContains(&it1, &it2));
}

Datum
jsonbc_ne(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) != 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

/*
 * B-Tree operator class operators, support function
 */
Datum
jsonbc_lt(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) < 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

Datum
jsonbc_gt(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) > 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

Datum
jsonbc_le(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) <= 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

Datum
jsonbc_ge(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) >= 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

Datum
jsonbc_eq(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	bool		res;

	res = (compareJsonbcContainers(&jba->root, &jbb->root) == 0);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_BOOL(res);
}

Datum
jsonbc_cmp(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jba = PG_GETARG_JSONB(0);
	Jsonbc	   *jbb = PG_GETARG_JSONB(1);
	int			res;

	res = compareJsonbcContainers(&jba->root, &jbb->root);

	PG_FREE_IF_COPY(jba, 0);
	PG_FREE_IF_COPY(jbb, 1);
	PG_RETURN_INT32(res);
}

/*
 * Hash operator class jsonbc hashing function
 */
Datum
jsonbc_hash(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	JsonbcIterator *it;
	int32		r;
	JsonbcValue	v;
	uint32		hash = 0;

	if (JB_ROOT_COUNT(jb) == 0)
		PG_RETURN_INT32(0);

	it = JsonbcIteratorInit(&jb->root);

	while ((r = JsonbcIteratorNext(&it, &v, false)) != WJB_DONE)
	{
		switch (r)
		{
				/* Rotation is left to JsonbcHashScalarValue() */
			case WJB_BEGIN_ARRAY:
				hash ^= JB_FARRAY;
				break;
			case WJB_BEGIN_OBJECT:
				hash ^= JB_FOBJECT;
				break;
			case WJB_KEY:
			case WJB_VALUE:
			case WJB_ELEM:
				JsonbcHashScalarValue(&v, &hash);
				break;
			case WJB_END_ARRAY:
			case WJB_END_OBJECT:
				break;
			default:
				elog(ERROR, "invalid JsonbcIteratorNext rc: %d", r);
		}
	}

	PG_FREE_IF_COPY(jb, 0);
	PG_RETURN_INT32(hash);
}
