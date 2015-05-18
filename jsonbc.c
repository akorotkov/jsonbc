/*-------------------------------------------------------------------------
 *
 * jsonbc.c
 *		I/O routines for jsonbc type
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonbc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "libpq/pqformat.h"
#include "jsonbc.h"
#include "utils/builtins.h"
#include "utils/json.h"
#include "utils/jsonapi.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(jsonbc_array_element);
PG_FUNCTION_INFO_V1(jsonbc_array_element_text);
PG_FUNCTION_INFO_V1(jsonbc_array_elements);
PG_FUNCTION_INFO_V1(jsonbc_array_elements_text);
PG_FUNCTION_INFO_V1(jsonbc_array_length);
PG_FUNCTION_INFO_V1(jsonbc_cmp);
PG_FUNCTION_INFO_V1(jsonbc_contained);
PG_FUNCTION_INFO_V1(jsonbc_contains);
PG_FUNCTION_INFO_V1(jsonbc_each);
PG_FUNCTION_INFO_V1(jsonbc_each_text);
PG_FUNCTION_INFO_V1(jsonbc_eq);
PG_FUNCTION_INFO_V1(jsonbc_exists);
PG_FUNCTION_INFO_V1(jsonbc_exists_all);
PG_FUNCTION_INFO_V1(jsonbc_exists_any);
PG_FUNCTION_INFO_V1(jsonbc_extract_path);
PG_FUNCTION_INFO_V1(jsonbc_extract_path_op);
PG_FUNCTION_INFO_V1(jsonbc_extract_path_text);
PG_FUNCTION_INFO_V1(jsonbc_extract_path_text_op);
PG_FUNCTION_INFO_V1(jsonbc_ge);
PG_FUNCTION_INFO_V1(jsonbc_gt);
PG_FUNCTION_INFO_V1(jsonbc_hash);
PG_FUNCTION_INFO_V1(jsonbc_in);
PG_FUNCTION_INFO_V1(jsonbc_le);
PG_FUNCTION_INFO_V1(jsonbc_lt);
PG_FUNCTION_INFO_V1(jsonbc_ne);
PG_FUNCTION_INFO_V1(jsonbc_object_field);
PG_FUNCTION_INFO_V1(jsonbc_object_field_text);
PG_FUNCTION_INFO_V1(jsonbc_object_keys);
PG_FUNCTION_INFO_V1(jsonbc_out);
PG_FUNCTION_INFO_V1(jsonbc_populate_record);
PG_FUNCTION_INFO_V1(jsonbc_populate_recordset);
PG_FUNCTION_INFO_V1(jsonbc_recv);
PG_FUNCTION_INFO_V1(jsonbc_send);
PG_FUNCTION_INFO_V1(jsonbc_to_record);
PG_FUNCTION_INFO_V1(jsonbc_to_recordset);
PG_FUNCTION_INFO_V1(jsonbc_typeof);
PG_FUNCTION_INFO_V1(gin_extract_jsonb);
PG_FUNCTION_INFO_V1(gin_extract_jsonbc_hash);
PG_FUNCTION_INFO_V1(gin_extract_jsonbc_query);
PG_FUNCTION_INFO_V1(gin_extract_jsonbc_query_hash);
PG_FUNCTION_INFO_V1(gin_consistent_jsonb);
PG_FUNCTION_INFO_V1(gin_consistent_jsonbc_hash);
PG_FUNCTION_INFO_V1(gin_compare_jsonb);

typedef struct JsonbcInState
{
	JsonbcParseState *parseState;
	JsonbcValue *res;
} JsonbcInState;

static inline Datum jsonbc_from_cstring(char *json, int len);
static size_t checkStringLen(size_t len);
static void jsonbc_in_object_start(void *pstate);
static void jsonbc_in_object_end(void *pstate);
static void jsonbc_in_array_start(void *pstate);
static void jsonbc_in_array_end(void *pstate);
static void jsonbc_in_object_field_start(void *pstate, char *fname, bool isnull);
static void jsonbc_put_escaped_value(StringInfo out, JsonbcValue *scalarVal);
static void jsonbc_in_scalar(void *pstate, char *token, JsonTokenType tokentype);

/*
 * jsonbc type input function
 */
Datum
jsonbc_in(PG_FUNCTION_ARGS)
{
	char	   *json = PG_GETARG_CSTRING(0);

	return jsonbc_from_cstring(json, strlen(json));
}

/*
 * jsonbc type recv function
 *
 * The type is sent as text in binary mode, so this is almost the same
 * as the input function, but it's prefixed with a version number so we
 * can change the binary format sent in future if necessary. For now,
 * only version 1 is supported.
 */
Datum
jsonbc_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	int			version = pq_getmsgint(buf, 1);
	char	   *str;
	int			nbytes;

	if (version == 1)
		str = pq_getmsgtext(buf, buf->len - buf->cursor, &nbytes);
	else
		elog(ERROR, "unsupported jsonbc version number %d", version);

	return jsonbc_from_cstring(str, nbytes);
}

/*
 * jsonbc type output function
 */
Datum
jsonbc_out(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	char	   *out;

	out = JsonbcToCString(NULL, &jb->root, VARSIZE(jb));

	PG_RETURN_CSTRING(out);
}

/*
 * jsonbc type send function
 *
 * Just send jsonbc as a version number, then a string of text
 */
Datum
jsonbc_send(PG_FUNCTION_ARGS)
{
	Jsonbc	   *jb = PG_GETARG_JSONB(0);
	StringInfoData buf;
	StringInfo	jtext = makeStringInfo();
	int			version = 1;

	(void) JsonbcToCString(jtext, &jb->root, VARSIZE(jb));

	pq_begintypsend(&buf);
	pq_sendint(&buf, version, 1);
	pq_sendtext(&buf, jtext->data, jtext->len);
	pfree(jtext->data);
	pfree(jtext);

	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*
 * SQL function jsonbc_typeof(jsonbc) -> text
 *
 * This function is here because the analog json function is in json.c, since
 * it uses the json parser internals not exposed elsewhere.
 */
Datum
jsonbc_typeof(PG_FUNCTION_ARGS)
{
	Jsonbc	   *in = PG_GETARG_JSONB(0);
	JsonbcIterator *it;
	JsonbcValue	v;
	char	   *result;

	if (JB_ROOT_IS_OBJECT(in))
		result = "object";
	else if (JB_ROOT_IS_ARRAY(in) && !JB_ROOT_IS_SCALAR(in))
		result = "array";
	else
	{
		Assert(JB_ROOT_IS_SCALAR(in));

		it = JsonbcIteratorInit(&in->root);

		/*
		 * A root scalar is stored as an array of one element, so we get the
		 * array and then its first (and only) member.
		 */
		(void) JsonbcIteratorNext(&it, &v, true);
		Assert(v.type == jbvArray);
		(void) JsonbcIteratorNext(&it, &v, true);
		switch (v.type)
		{
			case jbvNull:
				result = "null";
				break;
			case jbvString:
				result = "string";
				break;
			case jbvNumeric:
				result = "number";
				break;
			case jbvBool:
				result = "boolean";
				break;
			default:
				elog(ERROR, "unknown jsonbc scalar type");
		}
	}

	PG_RETURN_TEXT_P(cstring_to_text(result));
}

/*
 * jsonbc_from_cstring
 *
 * Turns json string into a jsonbc Datum.
 *
 * Uses the json parser (with hooks) to construct a jsonbc.
 */
static inline Datum
jsonbc_from_cstring(char *json, int len)
{
	JsonLexContext *lex;
	JsonbcInState state;
	JsonSemAction sem;

	memset(&state, 0, sizeof(state));
	memset(&sem, 0, sizeof(sem));
	lex = makeJsonLexContextCstringLen(json, len, true);

	sem.semstate = (void *) &state;

	sem.object_start = jsonbc_in_object_start;
	sem.array_start = jsonbc_in_array_start;
	sem.object_end = jsonbc_in_object_end;
	sem.array_end = jsonbc_in_array_end;
	sem.scalar = jsonbc_in_scalar;
	sem.object_field_start = jsonbc_in_object_field_start;

	pg_parse_json(lex, &sem);

	/* after parsing, the item member has the composed jsonbc structure */
	PG_RETURN_POINTER(JsonbcValueToJsonbc(state.res));
}

static size_t
checkStringLen(size_t len)
{
	if (len > JENTRY_OFFLENMASK)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("string too long to represent as jsonbc string"),
				 errdetail("Due to an implementation restriction, jsonbc strings cannot exceed %d bytes.",
						   JENTRY_OFFLENMASK)));

	return len;
}

static void
jsonbc_in_object_start(void *pstate)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;

	_state->res = pushJsonbcValue(&_state->parseState, WJB_BEGIN_OBJECT, NULL);
}

static void
jsonbc_in_object_end(void *pstate)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;

	_state->res = pushJsonbcValue(&_state->parseState, WJB_END_OBJECT, NULL);
}

static void
jsonbc_in_array_start(void *pstate)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;

	_state->res = pushJsonbcValue(&_state->parseState, WJB_BEGIN_ARRAY, NULL);
}

static void
jsonbc_in_array_end(void *pstate)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;

	_state->res = pushJsonbcValue(&_state->parseState, WJB_END_ARRAY, NULL);
}

static void
jsonbc_in_object_field_start(void *pstate, char *fname, bool isnull)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;
	JsonbcValue	v;

	Assert(fname != NULL);
	v.type = jbvString;
	v.val.string.len = checkStringLen(strlen(fname));
	v.val.string.val = fname;

	_state->res = pushJsonbcValue(&_state->parseState, WJB_KEY, &v);
}

static void
jsonbc_put_escaped_value(StringInfo out, JsonbcValue *scalarVal)
{
	switch (scalarVal->type)
	{
		case jbvNull:
			appendBinaryStringInfo(out, "null", 4);
			break;
		case jbvString:
			escape_json(out, pnstrdup(scalarVal->val.string.val, scalarVal->val.string.len));
			break;
		case jbvNumeric:
			appendStringInfoString(out,
							 DatumGetCString(DirectFunctionCall1(numeric_out,
								  PointerGetDatum(scalarVal->val.numeric))));
			break;
		case jbvBool:
			if (scalarVal->val.boolean)
				appendBinaryStringInfo(out, "true", 4);
			else
				appendBinaryStringInfo(out, "false", 5);
			break;
		default:
			elog(ERROR, "unknown jsonbc scalar type");
	}
}

/*
 * For jsonbc we always want the de-escaped value - that's what's in token
 */
static void
jsonbc_in_scalar(void *pstate, char *token, JsonTokenType tokentype)
{
	JsonbcInState *_state = (JsonbcInState *) pstate;
	JsonbcValue	v;

	switch (tokentype)
	{

		case JSON_TOKEN_STRING:
			Assert(token != NULL);
			v.type = jbvString;
			v.val.string.len = checkStringLen(strlen(token));
			v.val.string.val = token;
			break;
		case JSON_TOKEN_NUMBER:

			/*
			 * No need to check size of numeric values, because maximum
			 * numeric size is well below the JsonbcValue restriction
			 */
			Assert(token != NULL);
			v.type = jbvNumeric;
			v.val.numeric = DatumGetNumeric(DirectFunctionCall3(numeric_in, CStringGetDatum(token), 0, -1));

			break;
		case JSON_TOKEN_TRUE:
			v.type = jbvBool;
			v.val.boolean = true;
			break;
		case JSON_TOKEN_FALSE:
			v.type = jbvBool;
			v.val.boolean = false;
			break;
		case JSON_TOKEN_NULL:
			v.type = jbvNull;
			break;
		default:
			/* should not be possible */
			elog(ERROR, "invalid json token type");
			break;
	}

	if (_state->parseState == NULL)
	{
		/* single scalar */
		JsonbcValue	va;

		va.type = jbvArray;
		va.val.array.rawScalar = true;
		va.val.array.nElems = 1;

		_state->res = pushJsonbcValue(&_state->parseState, WJB_BEGIN_ARRAY, &va);
		_state->res = pushJsonbcValue(&_state->parseState, WJB_ELEM, &v);
		_state->res = pushJsonbcValue(&_state->parseState, WJB_END_ARRAY, NULL);
	}
	else
	{
		JsonbcValue *o = &_state->parseState->contVal;

		switch (o->type)
		{
			case jbvArray:
				_state->res = pushJsonbcValue(&_state->parseState, WJB_ELEM, &v);
				break;
			case jbvObject:
				_state->res = pushJsonbcValue(&_state->parseState, WJB_VALUE, &v);
				break;
			default:
				elog(ERROR, "unexpected parent of nested structure");
		}
	}
}

/*
 * JsonbcToCString
 *	   Converts jsonbc value to a C-string.
 *
 * If 'out' argument is non-null, the resulting C-string is stored inside the
 * StringBuffer.  The resulting string is always returned.
 *
 * A typical case for passing the StringInfo in rather than NULL is where the
 * caller wants access to the len attribute without having to call strlen, e.g.
 * if they are converting it to a text* object.
 */
char *
JsonbcToCString(StringInfo out, JsonbcContainer *in, int estimated_len)
{
	bool		first = true;
	JsonbcIterator *it;
	int			type = 0;
	JsonbcValue	v;
	int			level = 0;
	bool		redo_switch = false;

	if (out == NULL)
		out = makeStringInfo();

	enlargeStringInfo(out, (estimated_len >= 0) ? estimated_len : 64);

	it = JsonbcIteratorInit(in);

	while (redo_switch ||
		   ((type = JsonbcIteratorNext(&it, &v, false)) != WJB_DONE))
	{
		redo_switch = false;
		switch (type)
		{
			case WJB_BEGIN_ARRAY:
				if (!first)
					appendBinaryStringInfo(out, ", ", 2);
				first = true;

				if (!v.val.array.rawScalar)
					appendStringInfoChar(out, '[');
				level++;
				break;
			case WJB_BEGIN_OBJECT:
				if (!first)
					appendBinaryStringInfo(out, ", ", 2);
				first = true;
				appendStringInfoCharMacro(out, '{');

				level++;
				break;
			case WJB_KEY:
				if (!first)
					appendBinaryStringInfo(out, ", ", 2);
				first = true;

				/* json rules guarantee this is a string */
				jsonbc_put_escaped_value(out, &v);
				appendBinaryStringInfo(out, ": ", 2);

				type = JsonbcIteratorNext(&it, &v, false);
				if (type == WJB_VALUE)
				{
					first = false;
					jsonbc_put_escaped_value(out, &v);
				}
				else
				{
					Assert(type == WJB_BEGIN_OBJECT || type == WJB_BEGIN_ARRAY);

					/*
					 * We need to rerun the current switch() since we need to
					 * output the object which we just got from the iterator
					 * before calling the iterator again.
					 */
					redo_switch = true;
				}
				break;
			case WJB_ELEM:
				if (!first)
					appendBinaryStringInfo(out, ", ", 2);
				else
					first = false;

				jsonbc_put_escaped_value(out, &v);
				break;
			case WJB_END_ARRAY:
				level--;
				if (!v.val.array.rawScalar)
					appendStringInfoChar(out, ']');
				first = false;
				break;
			case WJB_END_OBJECT:
				level--;
				appendStringInfoCharMacro(out, '}');
				first = false;
				break;
			default:
				elog(ERROR, "unknown flag of jsonbc iterator");
		}
	}

	Assert(level == 0);

	return out->data;
}
