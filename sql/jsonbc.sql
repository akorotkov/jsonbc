CREATE EXTENSION jsonbc;

-- Strings.
SELECT '""'::jsonbc;				-- OK.
SELECT $$''$$::jsonbc;			-- ERROR, single quotes are not allowed
SELECT '"abc"'::jsonbc;			-- OK
SELECT '"abc'::jsonbc;			-- ERROR, quotes not closed
SELECT '"abc
def"'::jsonbc;					-- ERROR, unescaped newline in string constant
SELECT '"\n\"\\"'::jsonbc;		-- OK, legal escapes
SELECT '"\v"'::jsonbc;			-- ERROR, not a valid JSON escape
SELECT '"\u"'::jsonbc;			-- ERROR, incomplete escape
SELECT '"\u00"'::jsonbc;			-- ERROR, incomplete escape
SELECT '"\u000g"'::jsonbc;		-- ERROR, g is not a hex digit
SELECT '"\u0045"'::jsonbc;		-- OK, legal escape
SELECT '"\u0000"'::jsonbc;		-- ERROR, we don't support U+0000
-- use octet_length here so we don't get an odd unicode char in the
-- output
SELECT octet_length('"\uaBcD"'::jsonbc::text); -- OK, uppercase and lower case both OK

-- Numbers.
SELECT '1'::jsonbc;				-- OK
SELECT '0'::jsonbc;				-- OK
SELECT '01'::jsonbc;				-- ERROR, not valid according to JSON spec
SELECT '0.1'::jsonbc;				-- OK
SELECT '9223372036854775808'::jsonbc;	-- OK, even though it's too large for int8
SELECT '1e100'::jsonbc;			-- OK
SELECT '1.3e100'::jsonbc;			-- OK
SELECT '1f2'::jsonbc;				-- ERROR
SELECT '0.x1'::jsonbc;			-- ERROR
SELECT '1.3ex100'::jsonbc;		-- ERROR

-- Arrays.
SELECT '[]'::jsonbc;				-- OK
SELECT '[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]'::jsonbc;  -- OK
SELECT '[1,2]'::jsonbc;			-- OK
SELECT '[1,2,]'::jsonbc;			-- ERROR, trailing comma
SELECT '[1,2'::jsonbc;			-- ERROR, no closing bracket
SELECT '[1,[2]'::jsonbc;			-- ERROR, no closing bracket

-- Objects.
SELECT '{}'::jsonbc;				-- OK
SELECT '{"abc"}'::jsonbc;			-- ERROR, no value
SELECT '{"abc":1}'::jsonbc;		-- OK
SELECT '{1:"abc"}'::jsonbc;		-- ERROR, keys must be strings
SELECT '{"abc",1}'::jsonbc;		-- ERROR, wrong separator
SELECT '{"abc"=1}'::jsonbc;		-- ERROR, totally wrong separator
SELECT '{"abc"::1}'::jsonbc;		-- ERROR, another wrong separator
SELECT '{"abc":1,"def":2,"ghi":[3,4],"hij":{"klm":5,"nop":[6]}}'::jsonbc; -- OK
SELECT '{"abc":1:2}'::jsonbc;		-- ERROR, colon in wrong spot
SELECT '{"abc":1,3}'::jsonbc;		-- ERROR, no value

-- Miscellaneous stuff.
SELECT 'true'::jsonbc;			-- OK
SELECT 'false'::jsonbc;			-- OK
SELECT 'null'::jsonbc;			-- OK
SELECT ' true '::jsonbc;			-- OK, even with extra whitespace
SELECT 'true false'::jsonbc;		-- ERROR, too many values
SELECT 'true, false'::jsonbc;		-- ERROR, too many values
SELECT 'truf'::jsonbc;			-- ERROR, not a keyword
SELECT 'trues'::jsonbc;			-- ERROR, not a keyword
SELECT ''::jsonbc;				-- ERROR, no value
SELECT '    '::jsonbc;			-- ERROR, no value

-- jsonbc extraction functions
CREATE TEMP TABLE test_jsonbc (
       json_type text,
       test_json jsonbc
);

INSERT INTO test_jsonbc VALUES
('scalar','"a scalar"'),
('array','["zero", "one","two",null,"four","five", [1,2,3],{"f1":9}]'),
('object','{"field1":"val1","field2":"val2","field3":null, "field4": 4, "field5": [1,2,3], "field6": {"f1":9}}');

SELECT test_json -> 'x' FROM test_jsonbc WHERE json_type = 'scalar';
SELECT test_json -> 'x' FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json -> 'x' FROM test_jsonbc WHERE json_type = 'object';
SELECT test_json -> 'field2' FROM test_jsonbc WHERE json_type = 'object';

SELECT test_json ->> 'field2' FROM test_jsonbc WHERE json_type = 'scalar';
SELECT test_json ->> 'field2' FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json ->> 'field2' FROM test_jsonbc WHERE json_type = 'object';

SELECT test_json -> 2 FROM test_jsonbc WHERE json_type = 'scalar';
SELECT test_json -> 2 FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json -> 9 FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json -> 2 FROM test_jsonbc WHERE json_type = 'object';

SELECT test_json ->> 6 FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json ->> 7 FROM test_jsonbc WHERE json_type = 'array';

SELECT test_json ->> 'field4' FROM test_jsonbc WHERE json_type = 'object';
SELECT test_json ->> 'field5' FROM test_jsonbc WHERE json_type = 'object';
SELECT test_json ->> 'field6' FROM test_jsonbc WHERE json_type = 'object';

SELECT test_json ->> 2 FROM test_jsonbc WHERE json_type = 'scalar';
SELECT test_json ->> 2 FROM test_jsonbc WHERE json_type = 'array';
SELECT test_json ->> 2 FROM test_jsonbc WHERE json_type = 'object';

SELECT jsonbc_object_keys(test_json) FROM test_jsonbc WHERE json_type = 'scalar';
SELECT jsonbc_object_keys(test_json) FROM test_jsonbc WHERE json_type = 'array';
SELECT jsonbc_object_keys(test_json) FROM test_jsonbc WHERE json_type = 'object';

-- nulls
SELECT (test_json->'field3') IS NULL AS expect_false FROM test_jsonbc WHERE json_type = 'object';
SELECT (test_json->>'field3') IS NULL AS expect_true FROM test_jsonbc WHERE json_type = 'object';
SELECT (test_json->3) IS NULL AS expect_false FROM test_jsonbc WHERE json_type = 'array';
SELECT (test_json->>3) IS NULL AS expect_true FROM test_jsonbc WHERE json_type = 'array';

-- corner cases
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc -> null::text;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc -> null::int;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc -> 1;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc -> 'z';
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc -> '';
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc -> 1;
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc -> 3;
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc -> 'z';
select '{"a": "c", "b": null}'::jsonbc -> 'b';
select '"foo"'::jsonbc -> 1;
select '"foo"'::jsonbc -> 'z';

select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc ->> null::text;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc ->> null::int;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc ->> 1;
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc ->> 'z';
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc ->> '';
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc ->> 1;
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc ->> 3;
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc ->> 'z';
select '{"a": "c", "b": null}'::jsonbc ->> 'b';
select '"foo"'::jsonbc ->> 1;
select '"foo"'::jsonbc ->> 'z';

-- equality and inequality
SELECT '{"x":"y"}'::jsonbc = '{"x":"y"}'::jsonbc;
SELECT '{"x":"y"}'::jsonbc = '{"x":"z"}'::jsonbc;

SELECT '{"x":"y"}'::jsonbc <> '{"x":"y"}'::jsonbc;
SELECT '{"x":"y"}'::jsonbc <> '{"x":"z"}'::jsonbc;

-- containment
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"b"}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"b", "c":null}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"b", "g":null}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"g":null}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"c"}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"b"}');
SELECT jsonbc_contains('{"a":"b", "b":1, "c":null}', '{"a":"b", "c":"q"}');
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"b"}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"b", "c":null}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"b", "g":null}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"g":null}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"c"}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"b"}';
SELECT '{"a":"b", "b":1, "c":null}'::jsonbc @> '{"a":"b", "c":"q"}';

SELECT '[1,2]'::jsonbc @> '[1,2,2]'::jsonbc;
SELECT '[1,1,2]'::jsonbc @> '[1,2,2]'::jsonbc;
SELECT '[[1,2]]'::jsonbc @> '[[1,2,2]]'::jsonbc;
SELECT '[1,2,2]'::jsonbc <@ '[1,2]'::jsonbc;
SELECT '[1,2,2]'::jsonbc <@ '[1,1,2]'::jsonbc;
SELECT '[[1,2,2]]'::jsonbc <@ '[[1,2]]'::jsonbc;

SELECT jsonbc_contained('{"a":"b"}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"a":"b", "c":null}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"a":"b", "g":null}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"g":null}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"a":"c"}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"a":"b"}', '{"a":"b", "b":1, "c":null}');
SELECT jsonbc_contained('{"a":"b", "c":"q"}', '{"a":"b", "b":1, "c":null}');
SELECT '{"a":"b"}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"a":"b", "c":null}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"a":"b", "g":null}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"g":null}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"a":"c"}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"a":"b"}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
SELECT '{"a":"b", "c":"q"}'::jsonbc <@ '{"a":"b", "b":1, "c":null}';
-- Raw scalar may contain another raw scalar, array may contain a raw scalar
SELECT '[5]'::jsonbc @> '[5]';
SELECT '5'::jsonbc @> '5';
SELECT '[5]'::jsonbc @> '5';
-- But a raw scalar cannot contain an array
SELECT '5'::jsonbc @> '[5]';
-- In general, one thing should always contain itself. Test array containment:
SELECT '["9", ["7", "3"], 1]'::jsonbc @> '["9", ["7", "3"], 1]'::jsonbc;
SELECT '["9", ["7", "3"], ["1"]]'::jsonbc @> '["9", ["7", "3"], ["1"]]'::jsonbc;
-- array containment string matching confusion bug
SELECT '{ "name": "Bob", "tags": [ "enim", "qui"]}'::jsonbc @> '{"tags":["qu"]}';

-- array length
SELECT jsonbc_array_length('[1,2,3,{"f1":1,"f2":[5,6]},4]');
SELECT jsonbc_array_length('[]');
SELECT jsonbc_array_length('{"f1":1,"f2":[5,6]}');
SELECT jsonbc_array_length('4');

-- each
SELECT jsonbc_each('{"f1":[1,2,3],"f2":{"f3":1},"f4":null}');
SELECT jsonbc_each('{"a":{"b":"c","c":"b","1":"first"},"b":[1,2],"c":"cc","1":"first","n":null}'::jsonbc) AS q;
SELECT * FROM jsonbc_each('{"f1":[1,2,3],"f2":{"f3":1},"f4":null,"f5":99,"f6":"stringy"}') q;
SELECT * FROM jsonbc_each('{"a":{"b":"c","c":"b","1":"first"},"b":[1,2],"c":"cc","1":"first","n":null}'::jsonbc) AS q;

SELECT jsonbc_each_text('{"f1":[1,2,3],"f2":{"f3":1},"f4":null,"f5":"null"}');
SELECT jsonbc_each_text('{"a":{"b":"c","c":"b","1":"first"},"b":[1,2],"c":"cc","1":"first","n":null}'::jsonbc) AS q;
SELECT * FROM jsonbc_each_text('{"f1":[1,2,3],"f2":{"f3":1},"f4":null,"f5":99,"f6":"stringy"}') q;
SELECT * FROM jsonbc_each_text('{"a":{"b":"c","c":"b","1":"first"},"b":[1,2],"c":"cc","1":"first","n":null}'::jsonbc) AS q;

-- exists
SELECT jsonbc_exists('{"a":null, "b":"qq"}', 'a');
SELECT jsonbc_exists('{"a":null, "b":"qq"}', 'b');
SELECT jsonbc_exists('{"a":null, "b":"qq"}', 'c');
SELECT jsonbc_exists('{"a":"null", "b":"qq"}', 'a');
SELECT jsonbc '{"a":null, "b":"qq"}' ? 'a';
SELECT jsonbc '{"a":null, "b":"qq"}' ? 'b';
SELECT jsonbc '{"a":null, "b":"qq"}' ? 'c';
SELECT jsonbc '{"a":"null", "b":"qq"}' ? 'a';

CREATE TABLE testjsonbc (
       j jsonbc
);
\copy testjsonbc from 'data/jsonbc.data'

-- array exists - array elements should behave as keys
SELECT count(*) from testjsonbc  WHERE j->'array' ? 'bar';
-- type sensitive array exists - should return no rows (since "exists" only
-- matches strings that are either object keys or array elements)
SELECT count(*) from testjsonbc  WHERE j->'array' ? '5'::text;
-- However, a raw scalar is *contained* within the array
SELECT count(*) from testjsonbc  WHERE j->'array' @> '5'::jsonbc;

SELECT jsonbc_exists_any('{"a":null, "b":"qq"}', ARRAY['a','b']);
SELECT jsonbc_exists_any('{"a":null, "b":"qq"}', ARRAY['b','a']);
SELECT jsonbc_exists_any('{"a":null, "b":"qq"}', ARRAY['c','a']);
SELECT jsonbc_exists_any('{"a":null, "b":"qq"}', ARRAY['c','d']);
SELECT jsonbc_exists_any('{"a":null, "b":"qq"}', '{}'::text[]);
SELECT jsonbc '{"a":null, "b":"qq"}' ?| ARRAY['a','b'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?| ARRAY['b','a'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?| ARRAY['c','a'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?| ARRAY['c','d'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?| '{}'::text[];

SELECT jsonbc_exists_all('{"a":null, "b":"qq"}', ARRAY['a','b']);
SELECT jsonbc_exists_all('{"a":null, "b":"qq"}', ARRAY['b','a']);
SELECT jsonbc_exists_all('{"a":null, "b":"qq"}', ARRAY['c','a']);
SELECT jsonbc_exists_all('{"a":null, "b":"qq"}', ARRAY['c','d']);
SELECT jsonbc_exists_all('{"a":null, "b":"qq"}', '{}'::text[]);
SELECT jsonbc '{"a":null, "b":"qq"}' ?& ARRAY['a','b'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?& ARRAY['b','a'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?& ARRAY['c','a'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?& ARRAY['c','d'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?& ARRAY['a','a', 'b', 'b', 'b'];
SELECT jsonbc '{"a":null, "b":"qq"}' ?& '{}'::text[];

-- typeof
SELECT jsonbc_typeof('{}') AS object;
SELECT jsonbc_typeof('{"c":3,"p":"o"}') AS object;
SELECT jsonbc_typeof('[]') AS array;
SELECT jsonbc_typeof('["a", 1]') AS array;
SELECT jsonbc_typeof('null') AS "null";
SELECT jsonbc_typeof('1') AS number;
SELECT jsonbc_typeof('-1') AS number;
SELECT jsonbc_typeof('1.0') AS number;
SELECT jsonbc_typeof('1e2') AS number;
SELECT jsonbc_typeof('-1.0') AS number;
SELECT jsonbc_typeof('true') AS boolean;
SELECT jsonbc_typeof('false') AS boolean;
SELECT jsonbc_typeof('"hello"') AS string;
SELECT jsonbc_typeof('"true"') AS string;
SELECT jsonbc_typeof('"1.0"') AS string;

-- extract_path, extract_path_as_text
SELECT jsonbc_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}','f4','f6');
SELECT jsonbc_extract_path('{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}','f2');
SELECT jsonbc_extract_path('{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}','f2',0::text);
SELECT jsonbc_extract_path('{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}','f2',1::text);
SELECT jsonbc_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}','f4','f6');
SELECT jsonbc_extract_path_text('{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}','f2');
SELECT jsonbc_extract_path_text('{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}','f2',0::text);
SELECT jsonbc_extract_path_text('{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}','f2',1::text);

-- extract_path nulls
SELECT jsonbc_extract_path('{"f2":{"f3":1},"f4":{"f5":null,"f6":"stringy"}}','f4','f5') IS NULL AS expect_false;
SELECT jsonbc_extract_path_text('{"f2":{"f3":1},"f4":{"f5":null,"f6":"stringy"}}','f4','f5') IS NULL AS expect_true;
SELECT jsonbc_extract_path('{"f2":{"f3":1},"f4":[0,1,2,null]}','f4','3') IS NULL AS expect_false;
SELECT jsonbc_extract_path_text('{"f2":{"f3":1},"f4":[0,1,2,null]}','f4','3') IS NULL AS expect_true;

-- extract_path operators
SELECT '{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>array['f4','f6'];
SELECT '{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>array['f2'];
SELECT '{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>array['f2','0'];
SELECT '{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>array['f2','1'];

SELECT '{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>>array['f4','f6'];
SELECT '{"f2":{"f3":1},"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>>array['f2'];
SELECT '{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>>array['f2','0'];
SELECT '{"f2":["f3",1],"f4":{"f5":99,"f6":"stringy"}}'::jsonbc#>>array['f2','1'];

-- corner cases for same
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> '{}';
select '[1,2,3]'::jsonbc #> '{}';
select '"foo"'::jsonbc #> '{}';
select '42'::jsonbc #> '{}';
select 'null'::jsonbc #> '{}';
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a', null];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a', ''];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a','b'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a','b','c'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a','b','c','d'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #> array['a','z','c'];
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc #> array['a','1','b'];
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc #> array['a','z','b'];
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc #> array['1','b'];
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc #> array['z','b'];
select '[{"b": "c"}, {"b": null}]'::jsonbc #> array['1','b'];
select '"foo"'::jsonbc #> array['z'];
select '42'::jsonbc #> array['f2'];
select '42'::jsonbc #> array['0'];

select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> '{}';
select '[1,2,3]'::jsonbc #>> '{}';
select '"foo"'::jsonbc #>> '{}';
select '42'::jsonbc #>> '{}';
select 'null'::jsonbc #>> '{}';
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a', null];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a', ''];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a','b'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a','b','c'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a','b','c','d'];
select '{"a": {"b":{"c": "foo"}}}'::jsonbc #>> array['a','z','c'];
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc #>> array['a','1','b'];
select '{"a": [{"b": "c"}, {"b": "cc"}]}'::jsonbc #>> array['a','z','b'];
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc #>> array['1','b'];
select '[{"b": "c"}, {"b": "cc"}]'::jsonbc #>> array['z','b'];
select '[{"b": "c"}, {"b": null}]'::jsonbc #>> array['1','b'];
select '"foo"'::jsonbc #>> array['z'];
select '42'::jsonbc #>> array['f2'];
select '42'::jsonbc #>> array['0'];

-- array_elements
SELECT jsonbc_array_elements('[1,true,[1,[2,3]],null,{"f1":1,"f2":[7,8,9]},false]');
SELECT * FROM jsonbc_array_elements('[1,true,[1,[2,3]],null,{"f1":1,"f2":[7,8,9]},false]') q;
SELECT jsonbc_array_elements_text('[1,true,[1,[2,3]],null,{"f1":1,"f2":[7,8,9]},false,"stringy"]');
SELECT * FROM jsonbc_array_elements_text('[1,true,[1,[2,3]],null,{"f1":1,"f2":[7,8,9]},false,"stringy"]') q;

-- populate_record
CREATE TYPE jbpop AS (a text, b int, c timestamp);

SELECT * FROM jsonbc_populate_record(NULL::jbpop,'{"a":"blurfl","x":43.2}') q;
SELECT * FROM jsonbc_populate_record(row('x',3,'2012-12-31 15:30:56')::jbpop,'{"a":"blurfl","x":43.2}') q;

SELECT * FROM jsonbc_populate_record(NULL::jbpop,'{"a":"blurfl","x":43.2}') q;
SELECT * FROM jsonbc_populate_record(row('x',3,'2012-12-31 15:30:56')::jbpop,'{"a":"blurfl","x":43.2}') q;

SELECT * FROM jsonbc_populate_record(NULL::jbpop,'{"a":[100,200,false],"x":43.2}') q;
SELECT * FROM jsonbc_populate_record(row('x',3,'2012-12-31 15:30:56')::jbpop,'{"a":[100,200,false],"x":43.2}') q;
SELECT * FROM jsonbc_populate_record(row('x',3,'2012-12-31 15:30:56')::jbpop,'{"c":[100,200,false],"x":43.2}') q;

-- populate_recordset
SELECT * FROM jsonbc_populate_recordset(NULL::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(NULL::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"a":[100,200,300],"x":43.2},{"a":{"z":true},"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"c":[100,200,300],"x":43.2},{"a":{"z":true},"b":3,"c":"2012-01-20 10:42:53"}]') q;

SELECT * FROM jsonbc_populate_recordset(NULL::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"a":"blurfl","x":43.2},{"b":3,"c":"2012-01-20 10:42:53"}]') q;
SELECT * FROM jsonbc_populate_recordset(row('def',99,NULL)::jbpop,'[{"a":[100,200,300],"x":43.2},{"a":{"z":true},"b":3,"c":"2012-01-20 10:42:53"}]') q;

-- handling of unicode surrogate pairs

SELECT octet_length((jsonbc '{ "a":  "\ud83d\ude04\ud83d\udc36" }' -> 'a')::text) AS correct_in_utf8;
SELECT jsonbc '{ "a":  "\ud83d\ud83d" }' -> 'a'; -- 2 high surrogates in a row
SELECT jsonbc '{ "a":  "\ude04\ud83d" }' -> 'a'; -- surrogates in wrong order
SELECT jsonbc '{ "a":  "\ud83dX" }' -> 'a'; -- orphan high surrogate
SELECT jsonbc '{ "a":  "\ude04X" }' -> 'a'; -- orphan low surrogate

-- handling of simple unicode escapes

SELECT jsonbc '{ "a":  "the Copyright \u00a9 sign" }' as correct_in_utf8;
SELECT jsonbc '{ "a":  "dollar \u0024 character" }' as correct_everywhere;
SELECT jsonbc '{ "a":  "dollar \\u0024 character" }' as not_an_escape;
SELECT jsonbc '{ "a":  "null \u0000 escape" }' as fails;
SELECT jsonbc '{ "a":  "null \\u0000 escape" }' as not_an_escape;

SELECT jsonbc '{ "a":  "the Copyright \u00a9 sign" }' ->> 'a' as correct_in_utf8;
SELECT jsonbc '{ "a":  "dollar \u0024 character" }' ->> 'a' as correct_everywhere;
SELECT jsonbc '{ "a":  "dollar \\u0024 character" }' ->> 'a' as not_an_escape;
SELECT jsonbc '{ "a":  "null \u0000 escape" }' ->> 'a' as fails;
SELECT jsonbc '{ "a":  "null \\u0000 escape" }' ->> 'a' as not_an_escape;

-- jsonbc_to_record and jsonbc_to_recordset

select * from jsonbc_to_record('{"a":1,"b":"foo","c":"bar"}')
    as x(a int, b text, d text);

select * from jsonbc_to_recordset('[{"a":1,"b":"foo","d":false},{"a":2,"b":"bar","c":true}]')
    as x(a int, b text, c boolean);

-- indexing
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":null}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC"}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC", "public":true}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25.0}';
SELECT count(*) FROM testjsonbc WHERE j ? 'public';
SELECT count(*) FROM testjsonbc WHERE j ? 'bar';
SELECT count(*) FROM testjsonbc WHERE j ?| ARRAY['public','disabled'];
SELECT count(*) FROM testjsonbc WHERE j ?& ARRAY['public','disabled'];

CREATE INDEX jidx ON testjsonbc USING gin (j);
SET enable_seqscan = off;

SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":null}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC"}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC", "public":true}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25.0}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"array":["foo"]}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"array":["bar"]}';
-- excercise GIN_SEARCH_MODE_ALL
SELECT count(*) FROM testjsonbc WHERE j @> '{}';
SELECT count(*) FROM testjsonbc WHERE j ? 'public';
SELECT count(*) FROM testjsonbc WHERE j ? 'bar';
SELECT count(*) FROM testjsonbc WHERE j ?| ARRAY['public','disabled'];
SELECT count(*) FROM testjsonbc WHERE j ?& ARRAY['public','disabled'];

-- array exists - array elements should behave as keys (for GIN index scans too)
CREATE INDEX jidx_array ON testjsonbc USING gin((j->'array'));
SELECT count(*) from testjsonbc  WHERE j->'array' ? 'bar';
-- type sensitive array exists - should return no rows (since "exists" only
-- matches strings that are either object keys or array elements)
SELECT count(*) from testjsonbc  WHERE j->'array' ? '5'::text;
-- However, a raw scalar is *contained* within the array
SELECT count(*) from testjsonbc  WHERE j->'array' @> '5'::jsonbc;

RESET enable_seqscan;

SELECT count(*) FROM (SELECT (jsonbc_each(j)).key FROM testjsonbc) AS wow;
SELECT key, count(*) FROM (SELECT (jsonbc_each(j)).key FROM testjsonbc) AS wow GROUP BY key ORDER BY count DESC, key;

-- sort/hash
SELECT count(distinct j) FROM testjsonbc;
SET enable_hashagg = off;
SELECT count(*) FROM (SELECT j FROM (SELECT * FROM testjsonbc UNION ALL SELECT * FROM testjsonbc) js GROUP BY j) js2;
SET enable_hashagg = on;
SET enable_sort = off;
SELECT count(*) FROM (SELECT j FROM (SELECT * FROM testjsonbc UNION ALL SELECT * FROM testjsonbc) js GROUP BY j) js2;
SELECT distinct * FROM (values (jsonbc '{}' || ''),('{}')) v(j);
SET enable_sort = on;

RESET enable_hashagg;
RESET enable_sort;

DROP INDEX jidx;
DROP INDEX jidx_array;
-- btree
CREATE INDEX jidx ON testjsonbc USING btree (j);
SET enable_seqscan = off;

SELECT count(*) FROM testjsonbc WHERE j > '{"p":1}';
SELECT count(*) FROM testjsonbc WHERE j = '{"pos":98, "line":371, "node":"CBA", "indexed":true}';

--gin path opclass
DROP INDEX jidx;
CREATE INDEX jidx ON testjsonbc USING gin (j jsonbc_path_ops);
SET enable_seqscan = off;

SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":null}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC"}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"wait":"CC", "public":true}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25}';
SELECT count(*) FROM testjsonbc WHERE j @> '{"age":25.0}';
-- excercise GIN_SEARCH_MODE_ALL
SELECT count(*) FROM testjsonbc WHERE j @> '{}';

RESET enable_seqscan;
DROP INDEX jidx;

-- nested tests
SELECT '{"ff":{"a":12,"b":16}}'::jsonbc;
SELECT '{"ff":{"a":12,"b":16},"qq":123}'::jsonbc;
SELECT '{"aa":["a","aaa"],"qq":{"a":12,"b":16,"c":["c1","c2"],"d":{"d1":"d1","d2":"d2","d1":"d3"}}}'::jsonbc;
SELECT '{"aa":["a","aaa"],"qq":{"a":"12","b":"16","c":["c1","c2"],"d":{"d1":"d1","d2":"d2"}}}'::jsonbc;
SELECT '{"aa":["a","aaa"],"qq":{"a":"12","b":"16","c":["c1","c2",["c3"],{"c4":4}],"d":{"d1":"d1","d2":"d2"}}}'::jsonbc;
SELECT '{"ff":["a","aaa"]}'::jsonbc;

SELECT
  '{"ff":{"a":12,"b":16},"qq":123,"x":[1,2],"Y":null}'::jsonbc -> 'ff',
  '{"ff":{"a":12,"b":16},"qq":123,"x":[1,2],"Y":null}'::jsonbc -> 'qq',
  ('{"ff":{"a":12,"b":16},"qq":123,"x":[1,2],"Y":null}'::jsonbc -> 'Y') IS NULL AS f,
  ('{"ff":{"a":12,"b":16},"qq":123,"x":[1,2],"Y":null}'::jsonbc ->> 'Y') IS NULL AS t,
   '{"ff":{"a":12,"b":16},"qq":123,"x":[1,2],"Y":null}'::jsonbc -> 'x';

-- nested containment
SELECT '{"a":[1,2],"c":"b"}'::jsonbc @> '{"a":[1,2]}';
SELECT '{"a":[2,1],"c":"b"}'::jsonbc @> '{"a":[1,2]}';
SELECT '{"a":{"1":2},"c":"b"}'::jsonbc @> '{"a":[1,2]}';
SELECT '{"a":{"2":1},"c":"b"}'::jsonbc @> '{"a":[1,2]}';
SELECT '{"a":{"1":2},"c":"b"}'::jsonbc @> '{"a":{"1":2}}';
SELECT '{"a":{"2":1},"c":"b"}'::jsonbc @> '{"a":{"1":2}}';
SELECT '["a","b"]'::jsonbc @> '["a","b","c","b"]';
SELECT '["a","b","c","b"]'::jsonbc @> '["a","b"]';
SELECT '["a","b","c",[1,2]]'::jsonbc @> '["a",[1,2]]';
SELECT '["a","b","c",[1,2]]'::jsonbc @> '["b",[1,2]]';

SELECT '{"a":[1,2],"c":"b"}'::jsonbc @> '{"a":[1]}';
SELECT '{"a":[1,2],"c":"b"}'::jsonbc @> '{"a":[2]}';
SELECT '{"a":[1,2],"c":"b"}'::jsonbc @> '{"a":[3]}';

SELECT '{"a":[1,2,{"c":3,"x":4}],"c":"b"}'::jsonbc @> '{"a":[{"c":3}]}';
SELECT '{"a":[1,2,{"c":3,"x":4}],"c":"b"}'::jsonbc @> '{"a":[{"x":4}]}';
SELECT '{"a":[1,2,{"c":3,"x":4}],"c":"b"}'::jsonbc @> '{"a":[{"x":4},3]}';
SELECT '{"a":[1,2,{"c":3,"x":4}],"c":"b"}'::jsonbc @> '{"a":[{"x":4},1]}';

-- nested object field / array index lookup
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'n';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'a';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'b';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'c';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'd';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'd' -> '1';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 'e';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc -> 0; --expecting error

SELECT '["a","b","c",[1,2],null]'::jsonbc -> 0;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 1;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 2;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 3;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 3 -> 1;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 4;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> 5;
SELECT '["a","b","c",[1,2],null]'::jsonbc -> -1;

--nested path extraction
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{0}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{a}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c,0}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c,1}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c,2}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c,3}';
SELECT '{"a":"b","c":[1,2,3]}'::jsonbc #> '{c,-1}';

SELECT '[0,1,2,[3,4],{"5":"five"}]'::jsonbc #> '{0}';
SELECT '[0,1,2,[3,4],{"5":"five"}]'::jsonbc #> '{3}';
SELECT '[0,1,2,[3,4],{"5":"five"}]'::jsonbc #> '{4}';
SELECT '[0,1,2,[3,4],{"5":"five"}]'::jsonbc #> '{4,5}';

--nested exists
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'n';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'a';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'b';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'c';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'd';
SELECT '{"n":null,"a":1,"b":[1,2],"c":{"1":2},"d":{"1":[2,3]}}'::jsonbc ? 'e';
