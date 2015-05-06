-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jsonb" to load this file. \quit

CREATE OR REPLACE FUNCTION jsonb_in(cstring)
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_in'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_in(cstring) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonb_out(jsonb)
  RETURNS cstring AS
'MODULE_PATHNAME', 'jsonb_out'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_out(jsonb) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonb_recv(internal)
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_recv'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_recv(internal) IS 'I/O';

CREATE OR REPLACE FUNCTION jsonb_send(jsonb)
  RETURNS bytea AS
'MODULE_PATHNAME', 'jsonb_send'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_send(jsonb) IS 'I/O';

CREATE TYPE jsonb (
    INTERNALLENGTH = variable,
    INPUT = jsonb_in,
    OUTPUT = jsonb_out,
    RECEIVE = jsonb_recv,
    SEND = jsonb_send,
    CATEGORY = 'C',
    ALIGNMENT = int4,
    STORAGE = extended
);
COMMENT ON TYPE jsonb IS 'Binary JSON';


CREATE OR REPLACE FUNCTION jsonb_array_element(from_json jsonb, element_index integer)
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_array_element'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_array_element(jsonb, integer) IS 'implementation of -> operator';

CREATE OR REPLACE FUNCTION jsonb_array_element_text(from_json jsonb, element_index integer)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonb_array_element_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_array_element_text(jsonb, integer) IS 'implementation of ->> operator';

CREATE OR REPLACE FUNCTION jsonb_array_elements(IN from_json jsonb, OUT value jsonb)
  RETURNS SETOF jsonb AS
'MODULE_PATHNAME', 'jsonb_array_elements'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_array_elements(jsonb) IS 'elements of a jsonb array';

CREATE OR REPLACE FUNCTION jsonb_array_elements_text(IN from_json jsonb, OUT value text)
  RETURNS SETOF text AS
'MODULE_PATHNAME', 'jsonb_array_elements_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_array_elements_text(jsonb) IS 'elements of jsonb array';

CREATE OR REPLACE FUNCTION jsonb_array_length(jsonb)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonb_array_length'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_array_length(jsonb) IS 'length of jsonb array';

CREATE OR REPLACE FUNCTION jsonb_cmp(jsonb, jsonb)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonb_cmp'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_cmp(jsonb, jsonb) IS 'less-equal-greater';

CREATE OR REPLACE FUNCTION jsonb_contained(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_contained'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_contained(jsonb, jsonb) IS 'implementation of <@ operator';

CREATE OR REPLACE FUNCTION jsonb_contains(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_contains'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_contains(jsonb, jsonb) IS 'implementation of @> operator';

CREATE OR REPLACE FUNCTION jsonb_each(IN from_json jsonb, OUT key text, OUT value jsonb)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonb_each'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_each(jsonb) IS 'key value pairs of a jsonb object';

CREATE OR REPLACE FUNCTION jsonb_each_text(IN from_json jsonb, OUT key text, OUT value text)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonb_each_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_each_text(jsonb) IS 'key value pairs of a jsonb object';

CREATE OR REPLACE FUNCTION jsonb_eq(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_eq'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_eq(jsonb, jsonb) IS 'implementation of = operator';

CREATE OR REPLACE FUNCTION jsonb_exists(jsonb, text)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_exists'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_exists(jsonb, text) IS 'implementation of ? operator';

CREATE OR REPLACE FUNCTION jsonb_exists_all(jsonb, text[])
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_exists_all'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_exists_all(jsonb, text[]) IS 'implementation of ?& operator';

CREATE OR REPLACE FUNCTION jsonb_exists_any(jsonb, text[])
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_exists_any'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_exists_any(jsonb, text[]) IS 'implementation of ?| operator';

CREATE OR REPLACE FUNCTION jsonb_extract_path(IN from_json jsonb, VARIADIC path_elems text[])
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_extract_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_extract_path(jsonb, text[]) IS 'get value from jsonb with path elements';

CREATE OR REPLACE FUNCTION jsonb_extract_path_op(from_json jsonb, path_elems text[])
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_extract_path'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_extract_path_op(jsonb, text[]) IS 'implementation of #> operator';

CREATE OR REPLACE FUNCTION jsonb_extract_path_text(IN from_json jsonb, VARIADIC path_elems text[])
  RETURNS text AS
'MODULE_PATHNAME', 'jsonb_extract_path_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_extract_path_text(jsonb, text[]) IS 'get value from jsonb as text with path elements';

CREATE OR REPLACE FUNCTION jsonb_extract_path_text_op(from_json jsonb, path_elems text[])
  RETURNS text AS
'MODULE_PATHNAME', 'jsonb_extract_path_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_extract_path_text_op(jsonb, text[]) IS 'implementation of #>> operator';

CREATE OR REPLACE FUNCTION jsonb_ge(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_ge'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_ge(jsonb, jsonb) IS 'implementation of >= operator';

CREATE OR REPLACE FUNCTION jsonb_gt(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_gt'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_gt(jsonb, jsonb) IS 'implementation of > operator';

CREATE OR REPLACE FUNCTION jsonb_hash(jsonb)
  RETURNS integer AS
'MODULE_PATHNAME', 'jsonb_hash'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_hash(jsonb) IS 'hash';

CREATE OR REPLACE FUNCTION jsonb_le(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_le'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_le(jsonb, jsonb) IS 'implementation of <= operator';

CREATE OR REPLACE FUNCTION jsonb_lt(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_lt'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_lt(jsonb, jsonb) IS 'implementation of < operator';

CREATE OR REPLACE FUNCTION jsonb_ne(jsonb, jsonb)
  RETURNS boolean AS
'MODULE_PATHNAME', 'jsonb_ne'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_ne(jsonb, jsonb) IS 'implementation of <> operator';

CREATE OR REPLACE FUNCTION jsonb_object_field(from_json jsonb, field_name text)
  RETURNS jsonb AS
'MODULE_PATHNAME', 'jsonb_object_field'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_object_field(jsonb, text) IS 'implementation of -> operator';

CREATE OR REPLACE FUNCTION jsonb_object_field_text(from_json jsonb, field_name text)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonb_object_field_text'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_object_field_text(jsonb, text) IS 'implementation of ->> operator';

CREATE OR REPLACE FUNCTION jsonb_object_keys(jsonb)
  RETURNS SETOF text AS
'MODULE_PATHNAME', 'jsonb_object_keys'
  LANGUAGE C IMMUTABLE STRICT
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_object_keys(jsonb) IS 'get jsonb object keys';

CREATE OR REPLACE FUNCTION jsonb_populate_record(base anyelement, from_json jsonb, use_json_as_text boolean DEFAULT false)
  RETURNS anyelement AS
'MODULE_PATHNAME', 'jsonb_populate_record'
  LANGUAGE C STABLE
  COST 1;
COMMENT ON FUNCTION jsonb_populate_record(anyelement, jsonb, boolean) IS 'get record fields from a jsonb object';

CREATE OR REPLACE FUNCTION jsonb_populate_recordset(base anyelement, from_json jsonb, use_json_as_text boolean DEFAULT false)
  RETURNS SETOF anyelement AS
'MODULE_PATHNAME', 'jsonb_populate_recordset'
  LANGUAGE C STABLE
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_populate_recordset(anyelement, jsonb, boolean) IS 'get set of records with fields from a jsonb array of objects';

CREATE OR REPLACE FUNCTION jsonb_to_record(from_json jsonb, nested_as_text boolean DEFAULT false)
  RETURNS record AS
'MODULE_PATHNAME', 'jsonb_to_record'
  LANGUAGE C STABLE
  COST 1;
COMMENT ON FUNCTION jsonb_to_record(jsonb, boolean) IS 'get record fields from a json object';

CREATE OR REPLACE FUNCTION jsonb_to_recordset(from_json jsonb, nested_as_text boolean DEFAULT false)
  RETURNS SETOF record AS
'MODULE_PATHNAME', 'jsonb_to_recordset'
  LANGUAGE C STABLE
  COST 1
  ROWS 100;
COMMENT ON FUNCTION jsonb_to_recordset(jsonb, boolean) IS 'get set of records with fields from a json array of objects';

CREATE OR REPLACE FUNCTION jsonb_typeof(jsonb)
  RETURNS text AS
'MODULE_PATHNAME', 'jsonb_typeof'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION jsonb_typeof(jsonb) IS 'get the type of a jsonb value';

CREATE OR REPLACE FUNCTION gin_extract_jsonb(internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonb'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonb(internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonb_hash(internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonb_hash'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonb_hash(internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonb_query(anyarray, internal, smallint, internal, internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonb_query'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonb_query(anyarray, internal, smallint, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_extract_jsonb_query_hash(anyarray, internal, smallint, internal, internal, internal, internal)
  RETURNS internal AS
'MODULE_PATHNAME', 'gin_extract_jsonb_query_hash'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_extract_jsonb_query_hash(anyarray, internal, smallint, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_consistent_jsonb(internal, smallint, anyarray, integer, internal, internal, internal, internal)
  RETURNS boolean AS
'MODULE_PATHNAME', 'gin_consistent_jsonb'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_consistent_jsonb(internal, smallint, anyarray, integer, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_consistent_jsonb_hash(internal, smallint, anyarray, integer, internal, internal, internal, internal)
  RETURNS boolean AS
'MODULE_PATHNAME', 'gin_consistent_jsonb_hash'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_consistent_jsonb_hash(internal, smallint, anyarray, integer, internal, internal, internal, internal) IS 'GIN support';

CREATE OR REPLACE FUNCTION gin_compare_jsonb(text, text)
  RETURNS integer AS
'MODULE_PATHNAME', 'gin_compare_jsonb'
  LANGUAGE C IMMUTABLE STRICT
  COST 1;
COMMENT ON FUNCTION gin_compare_jsonb(text, text) IS 'GIN support';

CREATE OPERATOR #> (
    PROCEDURE = jsonb_extract_path_op,
    LEFTARG = jsonb,
    RIGHTARG = text[]
);
COMMENT ON OPERATOR #> (jsonb, text[]) IS 'get value from jsonb with path elements';

CREATE OPERATOR #>> (
    PROCEDURE = jsonb_extract_path_text_op,
    LEFTARG = jsonb,
    RIGHTARG = text[]
);
COMMENT ON OPERATOR #>> (jsonb, text[]) IS 'get value from jsonb as text with path elements';

CREATE OPERATOR -> (
    PROCEDURE = jsonb_object_field,
    LEFTARG = jsonb,
    RIGHTARG = text
);
COMMENT ON OPERATOR -> (jsonb, text) IS 'get jsonb object field';

CREATE OPERATOR -> (
    PROCEDURE = jsonb_array_element,
    LEFTARG = jsonb,
    RIGHTARG = integer
);
COMMENT ON OPERATOR -> (jsonb, integer) IS 'get jsonb array element';

CREATE OPERATOR ->> (
    PROCEDURE = jsonb_object_field_text,
    LEFTARG = jsonb,
    RIGHTARG = text
);
COMMENT ON OPERATOR ->> (jsonb, text) IS 'get jsonb object field as text';

CREATE OPERATOR ->> (
    PROCEDURE = jsonb_array_element_text,
    LEFTARG = jsonb,
    RIGHTARG = integer
);
COMMENT ON OPERATOR ->> (jsonb, integer) IS 'get jsonb array element as text';


CREATE OPERATOR < (
    PROCEDURE = jsonb_lt,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR < (jsonb, jsonb) IS 'less than';

CREATE OPERATOR <= (
    PROCEDURE = jsonb_le,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);
COMMENT ON OPERATOR <= (jsonb, jsonb) IS 'less than or equal to';

CREATE OPERATOR <> (
    PROCEDURE = jsonb_ne,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);
COMMENT ON OPERATOR <> (jsonb, jsonb) IS 'not equal';

CREATE OPERATOR <@ (
    PROCEDURE = jsonb_contained,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    NEGATOR = @>,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR <@ (jsonb, jsonb) IS 'contained';

CREATE OPERATOR = (
    PROCEDURE = jsonb_eq,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = =,
    NEGATOR = <>,
    MERGES,
    HASHES,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);
COMMENT ON OPERATOR = (jsonb, jsonb) IS 'equal';

CREATE OPERATOR > (
    PROCEDURE = jsonb_gt,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR > (jsonb, jsonb) IS 'greater than';

CREATE OPERATOR >= (
    PROCEDURE = jsonb_ge,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);
COMMENT ON OPERATOR >= (jsonb, jsonb) IS 'greater than or equal to';

CREATE OPERATOR ? (
    PROCEDURE = jsonb_exists,
    LEFTARG = jsonb,
    RIGHTARG = text,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ? (jsonb, text) IS 'exists';

CREATE OPERATOR ?& (
    PROCEDURE = jsonb_exists_all,
    LEFTARG = jsonb,
    RIGHTARG = text[],
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ?& (jsonb, text[]) IS 'exists all';

CREATE OPERATOR ?| (
    PROCEDURE = jsonb_exists_any,
    LEFTARG = jsonb,
    RIGHTARG = text[],
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR ?| (jsonb, text[]) IS 'exists any';

CREATE OPERATOR @> (
    PROCEDURE = jsonb_contains,
    LEFTARG = jsonb,
    RIGHTARG = jsonb,
    NEGATOR = <@,
    RESTRICT = contsel,
    JOIN = contjoinsel
);
COMMENT ON OPERATOR @> (jsonb, jsonb) IS 'contains';


CREATE OPERATOR CLASS jsonb_ops DEFAULT
   FOR TYPE jsonb USING hash AS
   OPERATOR 1  =,
   FUNCTION 1  jsonb_hash(jsonb);

CREATE OPERATOR CLASS jsonb_ops DEFAULT
   FOR TYPE jsonb USING btree AS
   OPERATOR 1  <,
   OPERATOR 2  <=,
   OPERATOR 3  =,
   OPERATOR 4  >=,
   OPERATOR 5  >,
   FUNCTION 1  jsonb_cmp(jsonb, jsonb);

CREATE OPERATOR CLASS jsonb_ops DEFAULT
   FOR TYPE jsonb USING gin AS
   OPERATOR 7  @>,
   OPERATOR 9  ?(jsonb, text),
   OPERATOR 10  ?|(jsonb, _text),
   OPERATOR 11  ?&(jsonb, _text),
   FUNCTION 1  gin_compare_jsonb(text, text),
   FUNCTION 2  gin_extract_jsonb(internal, internal, internal),
   FUNCTION 3  gin_extract_jsonb_query(anyarray, internal, smallint, internal, internal, internal, internal),
   FUNCTION 4  gin_consistent_jsonb(internal, smallint, anyarray, integer, internal, internal, internal, internal),
   STORAGE text;

CREATE OPERATOR CLASS jsonb_hash_ops
   FOR TYPE jsonb USING gin AS
   OPERATOR 7  @>,
   FUNCTION 1  btint4cmp(integer, integer),
   FUNCTION 2  gin_extract_jsonb_hash(internal, internal, internal),
   FUNCTION 3  gin_extract_jsonb_query_hash(anyarray, internal, smallint, internal, internal, internal, internal),
   FUNCTION 4  gin_consistent_jsonb_hash(internal, smallint, anyarray, integer, internal, internal, internal, internal),
   STORAGE int4;
