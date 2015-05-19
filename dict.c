/*
 * dict.c
 *
 *  Created on: 18 мая 2015 г.
 *      Author: smagen
 */

#include "postgres.h"
#include "access/hash.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

#include "dict.h"

bool initialized = false;
HTAB *idToNameHash, *nameToIdHash;
SPIPlanPtr savedPlanInsert = NULL;
SPIPlanPtr savedPlanSelect = NULL;

typedef struct
{
	int32	id;
	char   *name;
} IdToName;

typedef struct
{
	char   *name;
	int32	id;
} NameToId;

static uint32
name_hash(const void *key, Size keysize)
{
	const char *name = *((const char **)key);

	return DatumGetUInt32(hash_any((const unsigned char *)name, strlen(name)));
}

static int
name_match(const void *key1, const void *key2, Size keysize)
{
	const char *name1 = *((const char **)key1);
	const char *name2 = *((const char **)key2);

	return strcmp(name1, name2);
}

static void
checkInit()
{
	HASHCTL ctl;

	if (initialized)
		return;

	memset(&ctl, 0, sizeof(ctl));
	ctl.hash = tag_hash;
	ctl.hcxt = TopMemoryContext;
	ctl.keysize = sizeof(int32);
	ctl.entrysize = sizeof(IdToName);
	idToNameHash = hash_create("Id to name map", 1024, &ctl,
							 HASH_FUNCTION | HASH_CONTEXT | HASH_ELEM);

	memset(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(char *);
	ctl.entrysize = sizeof(NameToId);
	ctl.hash = name_hash;
	ctl.match = name_match;
	ctl.hcxt = TopMemoryContext;
	nameToIdHash = hash_create("Name to id map", 1024, &ctl,
					HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

	initialized = true;
}

static IdToName *
addEntry(int id, char *name)
{
	NameToId   *nameToId;
	IdToName   *idToName;
	bool		found;

	name = MemoryContextStrdup(TopMemoryContext, name);

	nameToId = (NameToId *) hash_search(nameToIdHash,
									 (const void *)&name,
									 HASH_ENTER, &found);
	nameToId->id = id;

	idToName = (IdToName *) hash_search(idToNameHash,
									 (const void *)&id,
									 HASH_ENTER, &found);
	idToName->name = name;

	return idToName;
}

int32
getIdByName(char *name)
{
	NameToId   *nameToId;
	bool		found;

	checkInit();

	nameToId = (NameToId *) hash_search(nameToIdHash,
									 (const void *)&name,
									 HASH_FIND, &found);
	if (found)
	{
		return nameToId->id;
	}
	else
	{
		Oid		argTypes[1] = {TEXTOID};
		Datum	args[1];
		bool	null;
		int		id;

		SPI_connect();

		if (!savedPlanInsert)
		{
			savedPlanInsert = SPI_prepare(
				"WITH select_data AS ( "
				"	SELECT id FROM jsonbc_dict WHERE name = $1 "
				"), "
				"insert_data AS ( "
				"	INSERT INTO jsonbc_dict (name) "
				"		(SELECT $1 WHERE NOT EXISTS "
				"			(SELECT id FROM select_data)) RETURNING id "
				") "
				"SELECT id FROM select_data "
				"	UNION ALL "
				"SELECT id FROM insert_data;", 1, argTypes);
			if (!savedPlanInsert)
				elog(ERROR, "Error preparing query");
			if (SPI_keepplan(savedPlanInsert))
				elog(ERROR, "Error keeping plan");
		}

		args[0] = CStringGetTextDatum(name);
		if (SPI_execute_plan(savedPlanInsert, args, NULL, false, 1) < 0 ||
				SPI_processed != 1)
			elog(ERROR, "Failed to insert into dictionary");

		id = DatumGetInt32(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &null));
		addEntry(id, name);

		SPI_finish();
		return id;
	}
}

char *
getNameById(int32 id)
{
	IdToName   *result;
	bool		found;

	checkInit();

	result = (IdToName *) hash_search(idToNameHash,
									 (const void *)&id,
									 HASH_FIND, &found);
	if (found)
	{
		return result->name;
	}
	else
	{
		Oid		argTypes[1] = {INT4OID};
		Datum	args[1];
		bool	null;
		char   *name;

		SPI_connect();

		if (!savedPlanSelect)
		{
			savedPlanSelect = SPI_prepare(
				"SELECT name FROM jsonbc_dict WHERE id = $1;", 1, argTypes);
			if (!savedPlanSelect)
				elog(ERROR, "Error preparing query");
			if (SPI_keepplan(savedPlanSelect))
				elog(ERROR, "Error keeping plan");
		}

		args[0] = Int32GetDatum(id);
		if (SPI_execute_plan(savedPlanSelect, args, NULL, false, 1) < 0)
			elog(ERROR, "Failed to select from dictionary");

		if (SPI_processed < 1)
		{
			SPI_finish();
			return NULL;
		}

		name = TextDatumGetCString(SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &null));
		result = addEntry(id, name);

		SPI_finish();
		return result->name;
	}
}

PG_FUNCTION_INFO_V1(get_id_by_name);
PG_FUNCTION_INFO_V1(get_name_by_id);

Datum
get_id_by_name(PG_FUNCTION_ARGS)
{
	text *name = PG_GETARG_TEXT_PP(0);
	int32	id;

	id = getIdByName(text_to_cstring(name));

	PG_RETURN_INT32(id);
}

Datum
get_name_by_id(PG_FUNCTION_ARGS)
{
	int32 id = PG_GETARG_INT32(0);
	char *name;

	name = getNameById(id);
	if (name)
		PG_RETURN_TEXT_P(cstring_to_text(name));
	else
		PG_RETURN_NULL();
}
