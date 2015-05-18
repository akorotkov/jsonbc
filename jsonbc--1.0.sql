-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jsonbc" to load this file. \quit

CREATE TABLE jsonbc_dict
(
	id serial PRIMARY KEY,
	name text NOT NULL,
	UNIQUE (name)
);

CREATE OR REPLACE FUNCTION get_id_by_name(text)
  RETURNS int AS
'MODULE_PATHNAME' LANGUAGE C VOLATILE STRICT;

CREATE OR REPLACE FUNCTION get_name_by_id(int)
  RETURNS text AS
'MODULE_PATHNAME' LANGUAGE C VOLATILE STRICT;

CREATE OR REPLACE FUNCTION jsonbc_in(cstring)
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_in'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_in(cstring) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonbc_out(jsonbc)
  RETURNS cstring AS
'MODULE_PATHNAME', 'jsonbc_out'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_out(jsonbc) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonbc_recv(internal)
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_recv'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_recv(internal) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonbc_send(jsonbc)
  RETURNS bytea AS
'MODULE_PATHNAME', 'jsonbc_send'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_send(jsonbc) IS 'I/O';

CREATE TYPE jsonbc (
    INTERNALLENGTH = variable,
    INPUT = jsonbc_in,
    OUTPUT = jsonbc_out,
    RECEIVE = jsonbc_recv,
    SEND = jsonbc_send,
    CATEGORY = 'C',
    ALIGNMENT = int4,
    STORAGE = extended
);
COMMENT ON TYPE jsonbc IS 'Binary JSON';


CREATE OR REPLACE FUNCTION jsonbc_array_element(from_json jsonbc, element_index integer)
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_array_element'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_array_element(jsonbc, integer) IS 'implementation of -> operator';

CREATE OR REPLACE FUNCTION jsonbc_array_element_text(from_json jsonbc, element_index integer)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonbc_array_element_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_array_element_text(jsonbc, integer) IS 'implementation of ->> operator';

CREATE OR REPLACE FUNCTION jsonbc_array_elements(IN from_json jsonbc, OUT value jsonbc)
  RETURNS SETOF jsonbc AS
'MODULE_PATHNAME', 'jsonbc_array_elements'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_array_elements(jsonbc) IS 'elements of a jsonbc array';

CREATE OR REPLACE FUNCTION jsonbc_array_elements_text(IN from_json jsonbc, OUT value text)
  RETURNS SETOF text AS
'MODULE_PATHNAME', 'jsonbc_array_elements_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_array_elements_text(jsonbc) IS 'elements of jsonbc array';

CREATE OR REPLACE FUNCTION jsonbc_array_length(jsonbc)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonbc_array_length'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_array_length(jsonbc) IS 'length of jsonbc array';

CREATE OR REPLACE FUNCTION jsonbc_cmp(jsonbc, jsonbc)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonbc_cmp'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_cmp(jsonbc, jsonbc) IS 'less-equal-greater';

CREATE OR REPLACE FUNCTION jsonbc_contained(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_contained'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_contained(jsonbc, jsonbc) IS 'implementation of <@ operator';

CREATE OR REPLACE FUNCTION jsonbc_contains(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_contains'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_contains(jsonbc, jsonbc) IS 'implementation of @> operator';

CREATE OR REPLACE FUNCTION jsonbc_each(IN from_json jsonbc, OUT key text, OUT value jsonbc)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonbc_each'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_each(jsonbc) IS 'key value pairs of a jsonbc object';

CREATE OR REPLACE FUNCTION jsonbc_each_text(IN from_json jsonbc, OUT key text, OUT value text)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonbc_each_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_each_text(jsonbc) IS 'key value pairs of a jsonbc object';

CREATE OR REPLACE FUNCTION jsonbc_eq(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_eq'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_eq(jsonbc, jsonbc) IS 'implementation of = operator';

CREATE OR REPLACE FUNCTION jsonbc_exists(jsonbc, text)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_exists'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_exists(jsonbc, text) IS 'implementation of ? operator';

CREATE OR REPLACE FUNCTION jsonbc_exists_all(jsonbc, text[])
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_exists_all'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_exists_all(jsonbc, text[]) IS 'implementation of ?& operator';

CREATE OR REPLACE FUNCTION jsonbc_exists_any(jsonbc, text[])
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_exists_any'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_exists_any(jsonbc, text[]) IS 'implementation of ?| operator';

CREATE OR REPLACE FUNCTION jsonbc_extract_path(IN from_json jsonbc, VARIADIC path_elems text[])
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_extract_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_extract_path(jsonbc, text[]) IS 'get value from jsonbc with path elements';

CREATE OR REPLACE FUNCTION jsonbc_extract_path_op(from_json jsonbc, path_elems text[])
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_extract_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_extract_path_op(jsonbc, text[]) IS 'implementation of #> operator';

CREATE OR REPLACE FUNCTION jsonbc_extract_path_text(IN from_json jsonbc, VARIADIC path_elems text[])
  RETURNS text AS
'MODULE_PATHNAME', 'jsonbc_extract_path_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_extract_path_text(jsonbc, text[]) IS 'get value from jsonbc as text with path elements';

CREATE OR REPLACE FUNCTION jsonbc_extract_path_text_op(from_json jsonbc, path_elems text[])
  RETURNS text AS
'MODULE_PATHNAME', 'jsonbc_extract_path_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_extract_path_text_op(jsonbc, text[]) IS 'implementation of #>> operator';

CREATE OR REPLACE FUNCTION jsonbc_ge(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_ge'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_ge(jsonbc, jsonbc) IS 'implementation of >= operator';

CREATE OR REPLACE FUNCTION jsonbc_gt(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_gt'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_gt(jsonbc, jsonbc) IS 'implementation of > operator';

CREATE OR REPLACE FUNCTION jsonbc_hash(jsonbc)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonbc_hash'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_hash(jsonbc) IS 'hash';

CREATE OR REPLACE FUNCTION jsonbc_le(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_le'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_le(jsonbc, jsonbc) IS 'implementation of <= operator';

CREATE OR REPLACE FUNCTION jsonbc_lt(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_lt'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_lt(jsonbc, jsonbc) IS 'implementation of < operator';

CREATE OR REPLACE FUNCTION jsonbc_ne(jsonbc, jsonbc)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonbc_ne'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_ne(jsonbc, jsonbc) IS 'implementation of <> operator';

CREATE OR REPLACE FUNCTION jsonbc_object_field(from_json jsonbc, field_name text)
  RETURNS jsonbc AS
'MODULE_PATHNAME', 'jsonbc_object_field'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_object_field(jsonbc, text) IS 'implementation of -> operator';

CREATE OR REPLACE FUNCTION jsonbc_object_field_text(from_json jsonbc, field_name text)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonbc_object_field_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_object_field_text(jsonbc, text) IS 'implementation of ->> operator';

CREATE OR REPLACE FUNCTION jsonbc_object_keys(jsonbc)
  RETURNS SETOF text AS
'MODULE_PATHNAME', 'jsonbc_object_keys'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_object_keys(jsonbc) IS 'get jsonbc object keys';

CREATE OR REPLACE FUNCTION jsonbc_populate_record(base anyelement, from_json jsonbc, use_json_as_text boolean DEFAULT false)
  RETURNS anyelement AS
'MODULE_PATHNAME', 'jsonbc_populate_record'
  LANGUAGE C STABLE
  COST 1;
COMMENT ON FUNCTION jsonbc_populate_record(anyelement, jsonbc, boolean) IS 'get record fields from a jsonbc object';

CREATE OR REPLACE FUNCTION jsonbc_populate_recordset(base anyelement, from_json jsonbc, use_json_as_text boolean DEFAULT false)
  RETURNS SETOF anyelement AS
'MODULE_PATHNAME', 'jsonbc_populate_recordset'
  LANGUAGE C STABLE
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_populate_recordset(anyelement, jsonbc, boolean) IS 'get set of records with fields from a jsonbc array of objects';

CREATE OR REPLACE FUNCTION jsonbc_to_record(from_json jsonbc, nested_as_text boolean DEFAULT false)
  RETURNS record AS
'MODULE_PATHNAME', 'jsonbc_to_record'
  LANGUAGE C STABLE
  COST 1;
COMMENT ON FUNCTION jsonbc_to_record(jsonbc, boolean) IS 'get record fields from a json object';

CREATE OR REPLACE FUNCTION jsonbc_to_recordset(from_json jsonbc, nested_as_text boolean DEFAULT false)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonbc_to_recordset'
  LANGUAGE C STABLE
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonbc_to_recordset(jsonbc, boolean) IS 'get set of records with fields from a json array of objects';

CREATE OR REPLACE FUNCTION jsonbc_typeof(jsonbc)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonbc_typeof'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonbc_typeof(jsonbc) IS 'get the type of a jsonbc value';

CREATE OR REPLACE FUNCTION gin_extract_jsonbc(internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonbc'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonbc(internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonbc_path(internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonbc_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonbc_path(internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonbc_query(anyarray, internal, smallint, internal, internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonbc_query'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonbc_query(anyarray, internal, smallint, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonbc_query_path(anyarray, internal, smallint, internal, internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonbc_query_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonbc_query_path(anyarray, internal, smallint, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_consistent_jsonbc(internal, smallint, anyarray, integer, internal, internal, internal, internal)
  RETURNS boolean AS
'MODULE_PATHNAME', 'gin_consistent_jsonbc'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_consistent_jsonbc(internal, smallint, anyarray, integer, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_consistent_jsonbc_path(internal, smallint, anyarray, integer, internal, internal, internal, internal)
  RETURNS boolean AS
'MODULE_PATHNAME', 'gin_consistent_jsonbc_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_consistent_jsonbc_path(internal, smallint, anyarray, integer, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_compare_jsonbc(text, text)
  RETURNS integer AS
'MODULE_PATHNAME', 'gin_compare_jsonbc'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_compare_jsonbc(text, text) IS 'GIN support';

CREATE OPERATOR #> (
    PROCEDURE = jsonbc_extract_path_op,
    LEFTARG = jsonbc,
    RIGHTARG = text[]
);
COMMENT ON OPERATOR #> (jsonbc, text[]) IS 'get value from jsonbc with path elements';

CREATE OPERATOR #>> (
    PROCEDURE = jsonbc_extract_path_text_op,
    LEFTARG = jsonbc,
    RIGHTARG = text[]
);
COMMENT ON OPERATOR #>> (jsonbc, text[]) IS 'get value from jsonbc as text with path elements';

CREATE OPERATOR -> (
    PROCEDURE = jsonbc_object_field,
    LEFTARG = jsonbc,
    RIGHTARG = text
);
COMMENT ON OPERATOR -> (jsonbc, text) IS 'get jsonbc object field';

CREATE OPERATOR -> (
    PROCEDURE = jsonbc_array_element,
    LEFTARG = jsonbc,
    RIGHTARG = integer
);
COMMENT ON OPERATOR -> (jsonbc, integer) IS 'get jsonbc array element';

CREATE OPERATOR ->> (
    PROCEDURE = jsonbc_object_field_text,
    LEFTARG = jsonbc,
    RIGHTARG = text
);
COMMENT ON OPERATOR ->> (jsonbc, text) IS 'get jsonbc object field as text';

CREATE OPERATOR ->> (
    PROCEDURE = jsonbc_array_element_text,
    LEFTARG = jsonbc,
    RIGHTARG = integer
);
COMMENT ON OPERATOR ->> (jsonbc, integer) IS 'get jsonbc array element as text';


CREATE OPERATOR < (
    PROCEDURE = jsonbc_lt,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR < (jsonbc, jsonbc) IS 'less than';

CREATE OPERATOR <= (
    PROCEDURE = jsonbc_le,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR <= (jsonbc, jsonbc) IS 'less than or equal to';

CREATE OPERATOR <> (
    PROCEDURE = jsonbc_ne,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);
COMMENT ON OPERATOR <> (jsonbc, jsonbc) IS 'not equal';

CREATE OPERATOR <@ (
    PROCEDURE = jsonbc_contained,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    NEGATOR = @>,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR <@ (jsonbc, jsonbc) IS 'contained';

CREATE OPERATOR = (
    PROCEDURE = jsonbc_eq,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = =,
    NEGATOR = <>,
    MERGES,
    HASHES,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);
COMMENT ON OPERATOR = (jsonbc, jsonbc) IS 'equal';

CREATE OPERATOR > (
    PROCEDURE = jsonbc_gt,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR > (jsonbc, jsonbc) IS 'greater than';

CREATE OPERATOR >= (
    PROCEDURE = jsonbc_ge,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR >= (jsonbc, jsonbc) IS 'greater than or equal to';

CREATE OPERATOR ? (
    PROCEDURE = jsonbc_exists,
    LEFTARG = jsonbc,
    RIGHTARG = text,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ? (jsonbc, text) IS 'exists';

CREATE OPERATOR ?& (
    PROCEDURE = jsonbc_exists_all,
    LEFTARG = jsonbc,
    RIGHTARG = text[],
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ?& (jsonbc, text[]) IS 'exists all';

CREATE OPERATOR ?| (
    PROCEDURE = jsonbc_exists_any,
    LEFTARG = jsonbc,
    RIGHTARG = text[],
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ?| (jsonbc, text[]) IS 'exists any';

CREATE OPERATOR @> (
    PROCEDURE = jsonbc_contains,
    LEFTARG = jsonbc,
    RIGHTARG = jsonbc,
    NEGATOR = <@,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR @> (jsonbc, jsonbc) IS 'contains';


CREATE OPERATOR CLASS jsonbc_ops DEFAULT
   FOR TYPE jsonbc USING hash AS
   OPERATOR 1  =,
   FUNCTION 1  jsonbc_hash(jsonbc);

CREATE OPERATOR CLASS jsonbc_ops DEFAULT
   FOR TYPE jsonbc USING btree AS
   OPERATOR 1  <,
   OPERATOR 2  <=,
   OPERATOR 3  =,
   OPERATOR 4  >=,
   OPERATOR 5  >,
   FUNCTION 1  jsonbc_cmp(jsonbc, jsonbc);

CREATE OPERATOR CLASS jsonbc_ops DEFAULT
   FOR TYPE jsonbc USING gin AS
   OPERATOR 7  @>,
   OPERATOR 9  ?(jsonbc, text),
   OPERATOR 10  ?|(jsonbc, _text),
   OPERATOR 11  ?&(jsonbc, _text),
   FUNCTION 1  gin_compare_jsonbc(text, text),
   FUNCTION 2  gin_extract_jsonbc(internal, internal, internal),
   FUNCTION 3  gin_extract_jsonbc_query(anyarray, internal, smallint, internal, internal, internal, internal),
   FUNCTION 4  gin_consistent_jsonbc(internal, smallint, anyarray, integer, internal, internal, internal, internal),
   STORAGE text;

CREATE OPERATOR CLASS jsonbc_path_ops
   FOR TYPE jsonbc USING gin AS
   OPERATOR 7  @>,
   FUNCTION 1  btint4cmp(integer, integer),
   FUNCTION 2  gin_extract_jsonbc_path(internal, internal, internal),
   FUNCTION 3  gin_extract_jsonbc_query_path(anyarray, internal, smallint, internal, internal, internal, internal),
   FUNCTION 4  gin_consistent_jsonbc_path(internal, smallint, anyarray, integer, internal, internal, internal, internal),
   STORAGE int4;
