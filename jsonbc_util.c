/*-------------------------------------------------------------------------
 *
 * jsonbc_util.c
 *	  converting between Jsonbc and JsonbcValues, and iterating.
 *
 * Copyright (c) 2014, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonbc_util.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "catalog/pg_collation.h"
#include "jsonbc.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "dict.h"

/*
 * Maximum number of elements in an array (or key/value pairs in an object).
 * This is limited by two things: the size of the JEntry array must fit
 * in MaxAllocSize, and the number of elements (or pairs) must fit in the bits
 * reserved for that in the JsonbcContainer.header field.
 *
 * (The total size of an array's or object's elements is also limited by
 * JENTRY_OFFLENMASK, but we're not concerned about that here.)
 */
#define JSONB_MAX_ELEMS (MaxAllocSize / sizeof(JsonbcValue))
#define JSONB_MAX_PAIRS (MaxAllocSize / sizeof(JsonbcPair))

static void fillJsonbcValue(JEntry entry,
			   char *base_addr, uint32 offset,
			   JsonbcValue *result);
static bool equalsJsonbcScalarValue(JsonbcValue *a, JsonbcValue *b);
static int	compareJsonbcScalarValue(JsonbcValue *a, JsonbcValue *b);
static Jsonbc *convertToJsonbc(JsonbcValue *val);
static void convertJsonbcValue(StringInfo buffer, JEntry *header, JsonbcValue *val, int level);
static void convertJsonbcArray(StringInfo buffer, JEntry *header, JsonbcValue *val, int level);
static void convertJsonbcObject(StringInfo buffer, JEntry *header, JsonbcValue *val, int level);
static void convertJsonbcScalar(StringInfo buffer, JEntry *header, JsonbcValue *scalarVal);

static int	reserveFromBuffer(StringInfo buffer, int len);
static void appendToBuffer(StringInfo buffer, const char *data, int len);
static void copyToBuffer(StringInfo buffer, int offset, const char *data, int len);
static short padBufferToInt(StringInfo buffer);

static JsonbcIterator *iteratorFromContainer(JsonbcContainer *container, JsonbcIterator *parent);
static JsonbcIterator *freeAndGetParent(JsonbcIterator *it);
static JsonbcParseState *pushState(JsonbcParseState **pstate);
static void appendKey(JsonbcParseState *pstate, JsonbcValue *scalarVal);
static void appendValue(JsonbcParseState *pstate, JsonbcValue *scalarVal);
static void appendElement(JsonbcParseState *pstate, JsonbcValue *scalarVal);
static int	lengthCompareJsonbcStringValue(const void *a, const void *b);
static int	compareJsonbcPair(const void *a, const void *b, void *arg);
static void uniqueifyJsonbcObject(JsonbcValue *object);

static int32
convertKeyNameToId(JsonbcValue *string)
{
	KeyName	keyName;

	keyName.s = string->val.string.val;
	keyName.len = string->val.string.len;

	return getIdByName(keyName);
}

#define MAX_VARBYTE_SIZE 5

/*
 * Varbyte-encode 'val' into *ptr. *ptr is incremented to next integer.
 */
static void
encode_varbyte(uint32 val, unsigned char **ptr)
{
	unsigned char *p = *ptr;

	while (val > 0x7F)
	{
		*(p++) = 0x80 | (val & 0x7F);
		val >>= 7;
	}
	*(p++) = (unsigned char) val;

	*ptr = p;
}

/*
 * Decode varbyte-encoded integer at *ptr. *ptr is incremented to next integer.
 */
static uint32
decode_varbyte(unsigned char **ptr)
{
	uint64		val;
	unsigned char *p = *ptr;
	uint64		c;

	c = *(p++);
	val = c & 0x7F;
	if (c & 0x80)
	{
		c = *(p++);
		val |= (c & 0x7F) << 7;
		if (c & 0x80)
		{
			c = *(p++);
			val |= (c & 0x7F) << 14;
			if (c & 0x80)
			{
				c = *(p++);
				val |= (c & 0x7F) << 21;
				if (c & 0x80)
				{
					c = *(p++);
					val |= c << 28; /* last byte, no continuation bit */
				}
			}
		}
	}

	*ptr = p;

	return val;
}

static int
varbyte_size(uint32 value)
{
	if (value < 0x80)
		return 1;
	else if (value < 0x4000)
		return 2;
	else if (value < 0x200000)
		return 3;
	else if (value < 0x10000000)
		return 4;
	else
		return 5;
}

/*
 * Turn an in-memory JsonbcValue into a Jsonbc for on-disk storage.
 *
 * There isn't a JsonbcToJsonbcValue(), because generally we find it more
 * convenient to directly iterate through the Jsonbc representation and only
 * really convert nested scalar values.  JsonbcIteratorNext() does this, so that
 * clients of the iteration code don't have to directly deal with the binary
 * representation (JsonbcDeepContains() is a notable exception, although all
 * exceptions are internal to this module).  In general, functions that accept
 * a JsonbcValue argument are concerned with the manipulation of scalar values,
 * or simple containers of scalar values, where it would be inconvenient to
 * deal with a great amount of other state.
 */
Jsonbc *
JsonbcValueToJsonbc(JsonbcValue *val)
{
	Jsonbc	   *out;

	if (IsAJsonbcScalar(val))
	{
		/* Scalar value */
		JsonbcParseState *pstate = NULL;
		JsonbcValue *res;
		JsonbcValue	scalarArray;

		scalarArray.type = jbvArray;
		scalarArray.val.array.rawScalar = true;
		scalarArray.val.array.nElems = 1;

		pushJsonbcValue(&pstate, WJB_BEGIN_ARRAY, &scalarArray);
		pushJsonbcValue(&pstate, WJB_ELEM, val);
		res = pushJsonbcValue(&pstate, WJB_END_ARRAY, NULL);

		out = convertToJsonbc(res);
	}
	else if (val->type == jbvObject || val->type == jbvArray)
	{
		out = convertToJsonbc(val);
	}
	else
	{
		Assert(val->type == jbvBinary);
		out = palloc(VARHDRSZ + val->val.binary.len);
		SET_VARSIZE(out, VARHDRSZ + val->val.binary.len);
		memcpy(VARDATA(out), val->val.binary.data, val->val.binary.len);
	}

	return out;
}


/*
 * Get the offset of the variable-length portion of a Jsonbc node within
 * the variable-length-data part of its container.  The node is identified
 * by index within the container's JEntry array.
 */
uint32
getJsonbcOffset(const JsonbcContainer *jc, int index)
{
#ifdef NOT_USED
	uint32		offset = 0;
	int			i;

	/*
	 * Start offset of this entry is equal to the end offset of the previous
	 * entry.  Walk backwards to the most recent entry stored as an end
	 * offset, returning that offset plus any lengths in between.
	 */
	for (i = index - 1; i >= 0; i--)
	{
		offset += JBE_OFFLENFLD(jc->children[i]);
		if (JBE_HAS_OFF(jc->children[i]))
			break;
	}

	return offset;
#endif
	return 0;
}

/*
 * BT comparator worker function.  Returns an integer less than, equal to, or
 * greater than zero, indicating whether a is less than, equal to, or greater
 * than b.  Consistent with the requirements for a B-Tree operator class
 *
 * Strings are compared lexically, in contrast with other places where we use a
 * much simpler comparator logic for searching through Strings.  Since this is
 * called from B-Tree support function 1, we're careful about not leaking
 * memory here.
 */
int
compareJsonbcContainers(JsonbcContainer *a, JsonbcContainer *b)
{
	JsonbcIterator *ita,
			   *itb;
	int			res = 0;

	ita = JsonbcIteratorInit(a);
	itb = JsonbcIteratorInit(b);

	do
	{
		JsonbcValue	va,
					vb;
		int			ra,
					rb;

		ra = JsonbcIteratorNext(&ita, &va, false);
		rb = JsonbcIteratorNext(&itb, &vb, false);

		if (ra == rb)
		{
			if (ra == WJB_DONE)
			{
				/* Decisively equal */
				break;
			}

			if (ra == WJB_END_ARRAY || ra == WJB_END_OBJECT)
			{
				/*
				 * There is no array or object to compare at this stage of
				 * processing.  jbvArray/jbvObject values are compared
				 * initially, at the WJB_BEGIN_ARRAY and WJB_BEGIN_OBJECT
				 * tokens.
				 */
				continue;
			}

			if (va.type == vb.type)
			{
				switch (va.type)
				{
					case jbvString:
					case jbvNull:
					case jbvNumeric:
					case jbvBool:
						res = compareJsonbcScalarValue(&va, &vb);
						break;
					case jbvArray:

						/*
						 * This could be a "raw scalar" pseudo array.  That's
						 * a special case here though, since we still want the
						 * general type-based comparisons to apply, and as far
						 * as we're concerned a pseudo array is just a scalar.
						 */
						if (va.val.array.rawScalar != vb.val.array.rawScalar)
							res = (va.val.array.rawScalar) ? -1 : 1;
						if (va.val.array.nElems != vb.val.array.nElems)
							res = (va.val.array.nElems > vb.val.array.nElems) ? 1 : -1;
						break;
					case jbvObject:
						if (va.val.object.nPairs != vb.val.object.nPairs)
							res = (va.val.object.nPairs > vb.val.object.nPairs) ? 1 : -1;
						break;
					case jbvBinary:
						elog(ERROR, "unexpected jbvBinary value");
				}
			}
			else
			{
				/* Type-defined order */
				res = (va.type > vb.type) ? 1 : -1;
			}
		}
		else
		{
			/*
			 * It's safe to assume that the types differed, and that the va
			 * and vb values passed were set.
			 *
			 * If the two values were of the same container type, then there'd
			 * have been a chance to observe the variation in the number of
			 * elements/pairs (when processing WJB_BEGIN_OBJECT, say). They're
			 * either two heterogeneously-typed containers, or a container and
			 * some scalar type.
			 *
			 * We don't have to consider the WJB_END_ARRAY and WJB_END_OBJECT
			 * cases here, because we would have seen the corresponding
			 * WJB_BEGIN_ARRAY and WJB_BEGIN_OBJECT tokens first, and
			 * concluded that they don't match.
			 */
			Assert(ra != WJB_END_ARRAY && ra != WJB_END_OBJECT);
			Assert(rb != WJB_END_ARRAY && rb != WJB_END_OBJECT);

			Assert(va.type != vb.type);
			Assert(va.type != jbvBinary);
			Assert(vb.type != jbvBinary);
			/* Type-defined order */
			res = (va.type > vb.type) ? 1 : -1;
		}
	}
	while (res == 0);

	while (ita != NULL)
	{
		JsonbcIterator *i = ita->parent;

		pfree(ita);
		ita = i;
	}
	while (itb != NULL)
	{
		JsonbcIterator *i = itb->parent;

		pfree(itb);
		itb = i;
	}

	return res;
}

/*
 * Find value in object (i.e. the "value" part of some key/value pair in an
 * object), or find a matching element if we're looking through an array.  Do
 * so on the basis of equality of the object keys only, or alternatively
 * element values only, with a caller-supplied value "key".  The "flags"
 * argument allows the caller to specify which container types are of interest.
 *
 * This exported utility function exists to facilitate various cases concerned
 * with "containment".  If asked to look through an object, the caller had
 * better pass a Jsonbc String, because their keys can only be strings.
 * Otherwise, for an array, any type of JsonbcValue will do.
 *
 * In order to proceed with the search, it is necessary for callers to have
 * both specified an interest in exactly one particular container type with an
 * appropriate flag, as well as having the pointed-to Jsonbc container be of
 * one of those same container types at the top level. (Actually, we just do
 * whichever makes sense to save callers the trouble of figuring it out - at
 * most one can make sense, because the container either points to an array
 * (possibly a "raw scalar" pseudo array) or an object.)
 *
 * Note that we can return a jbvBinary JsonbcValue if this is called on an
 * object, but we never do so on an array.  If the caller asks to look through
 * a container type that is not of the type pointed to by the container,
 * immediately fall through and return NULL.  If we cannot find the value,
 * return NULL.  Otherwise, return palloc()'d copy of value.
 */
JsonbcValue *
findJsonbcValueFromContainer(JsonbcContainer *container, uint32 flags,
							JsonbcValue *key)
{
#ifdef NOT_USED
	JEntry	   *children = container->children;
	int			count = (container->header & JB_CMASK);
	JsonbcValue *result;

	Assert((flags & ~(JB_FARRAY | JB_FOBJECT)) == 0);

	/* Quick out without a palloc cycle if object/array is empty */
	if (count <= 0)
		return NULL;

	result = palloc(sizeof(JsonbcValue));

	if (flags & JB_FARRAY & container->header)
	{
		char	   *base_addr = (char *) (children + count);
		uint32		offset = 0;
		int			i;

		for (i = 0; i < count; i++)
		{
			fillJsonbcValue(container, i, base_addr, offset, result);

			if (key->type == result->type)
			{
				if (equalsJsonbcScalarValue(key, result))
					return result;
			}

			JBE_ADVANCE_OFFSET(offset, children[i]);
		}
	}
	else if (flags & JB_FOBJECT & container->header)
	{
		/* Since this is an object, account for *Pairs* of Jentrys */
		char	   *base_addr = (char *) (children + count * 2);
		uint32		stopLow = 0,
					stopHigh = count;
		int32		keyId;

		keyId = convertKeyNameToId(key);

		/* Object key passed by caller must be a string */
		Assert(key->type == jbvString);

		/* Binary search on object/pair keys *only* */
		while (stopLow < stopHigh)
		{
			uint32		stopMiddle;
			int32		candidate;

			stopMiddle = stopLow + (stopHigh - stopLow) / 2;

			candidate = (int32) container->children[stopMiddle + count];

			if (candidate == keyId)
			{
				fillJsonbcValue(container, stopMiddle, base_addr,
							   getJsonbcOffset(container, stopMiddle),
							   result);

				return result;
			}
			else
			{
				if (candidate < keyId)
					stopLow = stopMiddle + 1;
				else
					stopHigh = stopMiddle;
			}
		}
	}

	/* Not found */
	pfree(result);
#endif
	return NULL;
}

/*
 * Get i-th value of a Jsonbc array.
 *
 * Returns palloc()'d copy of the value, or NULL if it does not exist.
 */
JsonbcValue *
getIthJsonbcValueFromContainer(JsonbcContainer *container, uint32 i)
{
#ifdef NOT_USED
	JsonbcValue *result;
	char	   *base_addr;
	uint32		nelements;

	if ((container->header & JB_FARRAY) == 0)
		elog(ERROR, "not a jsonbc array");

	nelements = container->header & JB_CMASK;
	base_addr = (char *) &container->children[nelements];

	if (i >= nelements)
		return NULL;

	result = palloc(sizeof(JsonbcValue));

	fillJsonbcValue(container, i, base_addr,
				   getJsonbcOffset(container, i),
				   result);

	return result;
#endif
	return NULL;
}

/*
 * A helper function to fill in a JsonbcValue to represent an element of an
 * array, or a key or value of an object.
 *
 * The node's JEntry is at container->children[index], and its variable-length
 * data is at base_addr + offset.  We make the caller determine the offset
 * since in many cases the caller can amortize that work across multiple
 * children.  When it can't, it can just call getJsonbcOffset().
 *
 * A nested array or object will be returned as jbvBinary, ie. it won't be
 * expanded.
 */
static void
fillJsonbcValue(JEntry entry, char *base_addr, uint32 offset,
			   JsonbcValue *result)
{
	if (JBE_ISNULL(entry))
	{
		result->type = jbvNull;
	}
	else if (JBE_ISSTRING(entry))
	{
		result->type = jbvString;
		result->val.string.val = base_addr + offset;
		result->val.string.len = (entry >> JENTRY_SHIFT);
		Assert(result->val.string.len >= 0);
	}
	else if (JBE_ISNUMERIC(entry))
	{
		result->type = jbvNumeric;
		result->val.numeric = (Numeric) (base_addr + offset/* + INTALIGN(offset)*/);
	}
	else if (JBE_ISINTEGER(entry))
	{
		unsigned char *ptr = (unsigned char *)base_addr + offset;
		result->type = jbvNumeric;
		result->val.numeric = small_to_numeric(decode_varbyte(&ptr));
	}
	else if (JBE_ISBOOL_TRUE(entry))
	{
		result->type = jbvBool;
		result->val.boolean = true;
	}
	else if (JBE_ISBOOL_FALSE(entry))
	{
		result->type = jbvBool;
		result->val.boolean = false;
	}
	else
	{
		Assert(JBE_ISCONTAINER(entry));
		result->type = jbvBinary;
		/* Remove alignment padding from data pointer and length */
		result->val.binary.data = (JsonbcContainer *) (base_addr + offset);
		result->val.binary.len = (entry >> JENTRY_SHIFT);
	}
}

/*
 * Push JsonbcValue into JsonbcParseState.
 *
 * Used when parsing JSON tokens to form Jsonbc, or when converting an in-memory
 * JsonbcValue to a Jsonbc.
 *
 * Initial state of *JsonbcParseState is NULL, since it'll be allocated here
 * originally (caller will get JsonbcParseState back by reference).
 *
 * Only sequential tokens pertaining to non-container types should pass a
 * JsonbcValue.  There is one exception -- WJB_BEGIN_ARRAY callers may pass a
 * "raw scalar" pseudo array to append that.
 */
JsonbcValue *
pushJsonbcValue(JsonbcParseState **pstate, JsonbcIteratorToken seq,
			   JsonbcValue *scalarVal)
{
	JsonbcValue *result = NULL;

	switch (seq)
	{
		case WJB_BEGIN_ARRAY:
			Assert(!scalarVal || scalarVal->val.array.rawScalar);
			*pstate = pushState(pstate);
			result = &(*pstate)->contVal;
			(*pstate)->contVal.type = jbvArray;
			(*pstate)->contVal.val.array.nElems = 0;
			(*pstate)->contVal.val.array.rawScalar = (scalarVal &&
											 scalarVal->val.array.rawScalar);
			if (scalarVal && scalarVal->val.array.nElems > 0)
			{
				/* Assume that this array is still really a scalar */
				Assert(scalarVal->type == jbvArray);
				(*pstate)->size = scalarVal->val.array.nElems;
			}
			else
			{
				(*pstate)->size = 4;
			}
			(*pstate)->contVal.val.array.elems = palloc(sizeof(JsonbcValue) *
														(*pstate)->size);
			break;
		case WJB_BEGIN_OBJECT:
			Assert(!scalarVal);
			*pstate = pushState(pstate);
			result = &(*pstate)->contVal;
			(*pstate)->contVal.type = jbvObject;
			(*pstate)->contVal.val.object.nPairs = 0;
			(*pstate)->size = 4;
			(*pstate)->contVal.val.object.pairs = palloc(sizeof(JsonbcPair) *
														 (*pstate)->size);
			break;
		case WJB_KEY:
			Assert(scalarVal->type == jbvString);
			appendKey(*pstate, scalarVal);
			break;
		case WJB_VALUE:
			Assert(IsAJsonbcScalar(scalarVal) ||
				   scalarVal->type == jbvBinary);
			appendValue(*pstate, scalarVal);
			break;
		case WJB_ELEM:
			Assert(IsAJsonbcScalar(scalarVal) ||
				   scalarVal->type == jbvBinary);
			appendElement(*pstate, scalarVal);
			break;
		case WJB_END_OBJECT:
			uniqueifyJsonbcObject(&(*pstate)->contVal);
			/* fall through! */
		case WJB_END_ARRAY:
			/* Steps here common to WJB_END_OBJECT case */
			Assert(!scalarVal);
			result = &(*pstate)->contVal;

			/*
			 * Pop stack and push current array/object as value in parent
			 * array/object
			 */
			*pstate = (*pstate)->next;
			if (*pstate)
			{
				switch ((*pstate)->contVal.type)
				{
					case jbvArray:
						appendElement(*pstate, result);
						break;
					case jbvObject:
						appendValue(*pstate, result);
						break;
					default:
						elog(ERROR, "invalid jsonbc container type");
				}
			}
			break;
		default:
			elog(ERROR, "unrecognized jsonbc sequential processing token");
	}

	return result;
}

/*
 * pushJsonbcValue() worker:  Iteration-like forming of Jsonbc
 */
static JsonbcParseState *
pushState(JsonbcParseState **pstate)
{
	JsonbcParseState *ns = palloc(sizeof(JsonbcParseState));

	ns->next = *pstate;
	return ns;
}

/*
 * pushJsonbcValue() worker:  Append a pair key to state when generating a Jsonbc
 */
static void
appendKey(JsonbcParseState *pstate, JsonbcValue *string)
{
	JsonbcValue *object = &pstate->contVal;

	Assert(object->type == jbvObject);
	Assert(string->type == jbvString);

	if (object->val.object.nPairs >= JSONB_MAX_PAIRS)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("number of jsonbc object pairs exceeds the maximum allowed (%zu)",
						JSONB_MAX_PAIRS)));

	if (object->val.object.nPairs >= pstate->size)
	{
		pstate->size *= 2;
		object->val.object.pairs = repalloc(object->val.object.pairs,
											sizeof(JsonbcPair) * pstate->size);
	}

	object->val.object.pairs[object->val.object.nPairs].key = convertKeyNameToId(string);
	object->val.object.pairs[object->val.object.nPairs].order = object->val.object.nPairs;
}

/*
 * pushJsonbcValue() worker:  Append a pair value to state when generating a
 * Jsonbc
 */
static void
appendValue(JsonbcParseState *pstate, JsonbcValue *scalarVal)
{
	JsonbcValue *object = &pstate->contVal;

	Assert(object->type == jbvObject);

	object->val.object.pairs[object->val.object.nPairs++].value = *scalarVal;
}

/*
 * pushJsonbcValue() worker:  Append an element to state when generating a Jsonbc
 */
static void
appendElement(JsonbcParseState *pstate, JsonbcValue *scalarVal)
{
	JsonbcValue *array = &pstate->contVal;

	Assert(array->type == jbvArray);

	if (array->val.array.nElems >= JSONB_MAX_ELEMS)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("number of jsonbc array elements exceeds the maximum allowed (%zu)",
						JSONB_MAX_ELEMS)));

	if (array->val.array.nElems >= pstate->size)
	{
		pstate->size *= 2;
		array->val.array.elems = repalloc(array->val.array.elems,
										  sizeof(JsonbcValue) * pstate->size);
	}

	array->val.array.elems[array->val.array.nElems++] = *scalarVal;
}

/*
 * Given a JsonbcContainer, expand to JsonbcIterator to iterate over items
 * fully expanded to in-memory representation for manipulation.
 *
 * See JsonbcIteratorNext() for notes on memory management.
 */
JsonbcIterator *
JsonbcIteratorInit(JsonbcContainer *container)
{
	return iteratorFromContainer(container, NULL);
}

/*
 * Get next JsonbcValue while iterating
 *
 * Caller should initially pass their own, original iterator.  They may get
 * back a child iterator palloc()'d here instead.  The function can be relied
 * on to free those child iterators, lest the memory allocated for highly
 * nested objects become unreasonable, but only if callers don't end iteration
 * early (by breaking upon having found something in a search, for example).
 *
 * Callers in such a scenario, that are particularly sensitive to leaking
 * memory in a long-lived context may walk the ancestral tree from the final
 * iterator we left them with to its oldest ancestor, pfree()ing as they go.
 * They do not have to free any other memory previously allocated for iterators
 * but not accessible as direct ancestors of the iterator they're last passed
 * back.
 *
 * Returns "Jsonbc sequential processing" token value.  Iterator "state"
 * reflects the current stage of the process in a less granular fashion, and is
 * mostly used here to track things internally with respect to particular
 * iterators.
 *
 * Clients of this function should not have to handle any jbvBinary values
 * (since recursive calls will deal with this), provided skipNested is false.
 * It is our job to expand the jbvBinary representation without bothering them
 * with it.  However, clients should not take it upon themselves to touch array
 * or Object element/pair buffers, since their element/pair pointers are
 * garbage.  Also, *val will not be set when returning WJB_END_ARRAY or
 * WJB_END_OBJECT, on the assumption that it's only useful to access values
 * when recursing in.
 */
JsonbcIteratorToken
JsonbcIteratorNext(JsonbcIterator **it, JsonbcValue *val, bool skipNested)
{
	JEntry entry;
	uint32 keyIncr;

	if (*it == NULL)
		return WJB_DONE;

	/*
	 * When stepping into a nested container, we jump back here to start
	 * processing the child. We will not recurse further in one call, because
	 * processing the child will always begin in JBI_ARRAY_START or
	 * JBI_OBJECT_START state.
	 */
recurse:
	switch ((*it)->state)
	{
		case JBI_ARRAY_START:
			/* Set v to array on first array call */
			val->type = jbvArray;
			val->val.array.nElems = (*it)->childrenSize; /* FIXME */

			/*
			 * v->val.array.elems is not actually set, because we aren't doing
			 * a full conversion
			 */
			val->val.array.rawScalar = (*it)->isScalar;
			(*it)->childrenPtr = (*it)->children;
			(*it)->curDataOffset = 0;
			(*it)->curValueOffset = 0;	/* not actually used */
			/* Set state for next call */
			(*it)->state = JBI_ARRAY_ELEM;
			return WJB_BEGIN_ARRAY;

		case JBI_ARRAY_ELEM:
			if ((*it)->childrenPtr >= (*it)->children + (*it)->childrenSize)
			{
				/*
				 * All elements within array already processed.  Report this
				 * to caller, and give it back original parent iterator (which
				 * independently tracks iteration progress at its level of
				 * nesting).
				 */
				*it = freeAndGetParent(*it);
				return WJB_END_ARRAY;
			}

			entry = decode_varbyte(&(*it)->childrenPtr);

			fillJsonbcValue(entry,
						   (*it)->dataProper, (*it)->curDataOffset,
						   val);

			(*it)->curDataOffset += (entry >> JENTRY_SHIFT);

			if (!IsAJsonbcScalar(val) && !skipNested)
			{
				/* Recurse into container. */
				*it = iteratorFromContainer(val->val.binary.data, *it);
				goto recurse;
			}
			else
			{
				/*
				 * Scalar item in array, or a container and caller didn't want
				 * us to recurse into it.
				 */
				return WJB_ELEM;
			}

		case JBI_OBJECT_START:
			/* Set v to object on first object call */
			val->type = jbvObject;
			val->val.object.nPairs = (*it)->childrenSize; /* FIXME */

			/*
			 * v->val.object.pairs is not actually set, because we aren't
			 * doing a full conversion
			 */
			(*it)->childrenPtr = (*it)->children;
			(*it)->curKey = 0;
			(*it)->curDataOffset = 0;
			(*it)->curValueOffset = 0;	/* not actually used */
			/* Set state for next call */
			(*it)->state = JBI_OBJECT_KEY;
			return WJB_BEGIN_OBJECT;

		case JBI_OBJECT_KEY:
			if ((*it)->childrenPtr >= (*it)->children + (*it)->childrenSize)
			{
				/*
				 * All pairs within object already processed.  Report this to
				 * caller, and give it back original containing iterator
				 * (which independently tracks iteration progress at its level
				 * of nesting).
				 */
				*it = freeAndGetParent(*it);
				return WJB_END_OBJECT;
			}
			else
			{
				keyIncr = decode_varbyte(&(*it)->childrenPtr);
				(*it)->curKey += keyIncr;

				KeyName keyName = getNameById((*it)->curKey);

				val->type = jbvString;
				val->val.string.val = keyName.s;
				val->val.string.len = keyName.len;

				/* Set state for next call */
				(*it)->state = JBI_OBJECT_VALUE;
				return WJB_KEY;
			}

		case JBI_OBJECT_VALUE:
			/* Set state for next call */
			(*it)->state = JBI_OBJECT_KEY;

			entry = decode_varbyte(&(*it)->childrenPtr);

			fillJsonbcValue(entry,
						   (*it)->dataProper, (*it)->curDataOffset,
						   val);

			(*it)->curDataOffset += (entry >> JENTRY_SHIFT);

			/*
			 * Value may be a container, in which case we recurse with new,
			 * child iterator (unless the caller asked not to, by passing
			 * skipNested).
			 */
			if (!IsAJsonbcScalar(val) && !skipNested)
			{
				*it = iteratorFromContainer(val->val.binary.data, *it);
				goto recurse;
			}
			else
				return WJB_VALUE;
	}

	elog(ERROR, "invalid iterator state");
	return -1;
}

/*
 * Initialize an iterator for iterating all elements in a container.
 */
static JsonbcIterator *
iteratorFromContainer(JsonbcContainer *container, JsonbcIterator *parent)
{
	JsonbcIterator *it;
	uint32			header;
	unsigned char  *ptr;

	ptr = (unsigned char *)container->data;
	header = decode_varbyte(&ptr);

	it = palloc(sizeof(JsonbcIterator));
	it->container = container;
	it->parent = parent;
	it->childrenSize = (header >> JB_CSHIFT);

	/* Array starts just after header */
	it->children = ptr;
	it->dataProper = (char *)(ptr + it->childrenSize);

	switch (header & JB_MASK)
	{
		case JB_FSCALAR:
			Assert(it->nElems == 1);
			it->state = JBI_ARRAY_START;
			it->isScalar = true;
			break;
		case JB_FARRAY:
			it->state = JBI_ARRAY_START;
			it->isScalar = false;
			break;

		case JB_FOBJECT:
			it->state = JBI_OBJECT_START;
			break;

		default:
			elog(ERROR, "unknown type of jsonbc container");
	}

	return it;
}

/*
 * JsonbcIteratorNext() worker:	Return parent, while freeing memory for current
 * iterator
 */
static JsonbcIterator *
freeAndGetParent(JsonbcIterator *it)
{
	JsonbcIterator *v = it->parent;

	pfree(it);
	return v;
}

/*
 * Worker for "contains" operator's function
 *
 * Formally speaking, containment is top-down, unordered subtree isomorphism.
 *
 * Takes iterators that belong to some container type.  These iterators
 * "belong" to those values in the sense that they've just been initialized in
 * respect of them by the caller (perhaps in a nested fashion).
 *
 * "val" is lhs Jsonbc, and mContained is rhs Jsonbc when called from top level.
 * We determine if mContained is contained within val.
 */
bool
JsonbcDeepContains(JsonbcIterator **val, JsonbcIterator **mContained)
{
	uint32		rval,
				rcont;
	JsonbcValue	vval,
				vcontained;

	/*
	 * Guard against stack overflow due to overly complex Jsonbc.
	 *
	 * Functions called here independently take this precaution, but that
	 * might not be sufficient since this is also a recursive function.
	 */
	check_stack_depth();

	rval = JsonbcIteratorNext(val, &vval, false);
	rcont = JsonbcIteratorNext(mContained, &vcontained, false);

	if (rval != rcont)
	{
		/*
		 * The differing return values can immediately be taken as indicating
		 * two differing container types at this nesting level, which is
		 * sufficient reason to give up entirely (but it should be the case
		 * that they're both some container type).
		 */
		Assert(rval == WJB_BEGIN_OBJECT || rval == WJB_BEGIN_ARRAY);
		Assert(rcont == WJB_BEGIN_OBJECT || rcont == WJB_BEGIN_ARRAY);
		return false;
	}
	else if (rcont == WJB_BEGIN_OBJECT)
	{
		Assert(vval.type == jbvObject);
		Assert(vcontained.type == jbvObject);

		/*
		 * If the lhs has fewer pairs than the rhs, it can't possibly contain
		 * the rhs.  (This conclusion is safe only because we de-duplicate
		 * keys in all Jsonbc objects; thus there can be no corresponding
		 * optimization in the array case.)  The case probably won't arise
		 * often, but since it's such a cheap check we may as well make it.
		 */
		if (vval.val.object.nPairs < vcontained.val.object.nPairs)
			return false;

		/* Work through rhs "is it contained within?" object */
		for (;;)
		{
			JsonbcValue *lhsVal; /* lhsVal is from pair in lhs object */

			rcont = JsonbcIteratorNext(mContained, &vcontained, false);

			/*
			 * When we get through caller's rhs "is it contained within?"
			 * object without failing to find one of its values, it's
			 * contained.
			 */
			if (rcont == WJB_END_OBJECT)
				return true;

			Assert(rcont == WJB_KEY);

			/* First, find value by key... */
			lhsVal = findJsonbcValueFromContainer((*val)->container,
												 JB_FOBJECT,
												 &vcontained);

			if (!lhsVal)
				return false;

			/*
			 * ...at this stage it is apparent that there is at least a key
			 * match for this rhs pair.
			 */
			rcont = JsonbcIteratorNext(mContained, &vcontained, true);

			Assert(rcont == WJB_VALUE);

			/*
			 * Compare rhs pair's value with lhs pair's value just found using
			 * key
			 */
			if (lhsVal->type != vcontained.type)
			{
				return false;
			}
			else if (IsAJsonbcScalar(lhsVal))
			{
				if (!equalsJsonbcScalarValue(lhsVal, &vcontained))
					return false;
			}
			else
			{
				/* Nested container value (object or array) */
				JsonbcIterator *nestval,
						   *nestContained;

				Assert(lhsVal->type == jbvBinary);
				Assert(vcontained.type == jbvBinary);

				nestval = JsonbcIteratorInit(lhsVal->val.binary.data);
				nestContained = JsonbcIteratorInit(vcontained.val.binary.data);

				/*
				 * Match "value" side of rhs datum object's pair recursively.
				 * It's a nested structure.
				 *
				 * Note that nesting still has to "match up" at the right
				 * nesting sub-levels.  However, there need only be zero or
				 * more matching pairs (or elements) at each nesting level
				 * (provided the *rhs* pairs/elements *all* match on each
				 * level), which enables searching nested structures for a
				 * single String or other primitive type sub-datum quite
				 * effectively (provided the user constructed the rhs nested
				 * structure such that we "know where to look").
				 *
				 * In other words, the mapping of container nodes in the rhs
				 * "vcontained" Jsonbc to internal nodes on the lhs is
				 * injective, and parent-child edges on the rhs must be mapped
				 * to parent-child edges on the lhs to satisfy the condition
				 * of containment (plus of course the mapped nodes must be
				 * equal).
				 */
				if (!JsonbcDeepContains(&nestval, &nestContained))
					return false;
			}
		}
	}
	else if (rcont == WJB_BEGIN_ARRAY)
	{
		JsonbcValue *lhsConts = NULL;
		uint32		nLhsElems = vval.val.array.nElems;

		Assert(vval.type == jbvArray);
		Assert(vcontained.type == jbvArray);

		/*
		 * Handle distinction between "raw scalar" pseudo arrays, and real
		 * arrays.
		 *
		 * A raw scalar may contain another raw scalar, and an array may
		 * contain a raw scalar, but a raw scalar may not contain an array. We
		 * don't do something like this for the object case, since objects can
		 * only contain pairs, never raw scalars (a pair is represented by an
		 * rhs object argument with a single contained pair).
		 */
		if (vval.val.array.rawScalar && !vcontained.val.array.rawScalar)
			return false;

		/* Work through rhs "is it contained within?" array */
		for (;;)
		{
			rcont = JsonbcIteratorNext(mContained, &vcontained, true);

			/*
			 * When we get through caller's rhs "is it contained within?"
			 * array without failing to find one of its values, it's
			 * contained.
			 */
			if (rcont == WJB_END_ARRAY)
				return true;

			Assert(rcont == WJB_ELEM);

			if (IsAJsonbcScalar(&vcontained))
			{
				if (!findJsonbcValueFromContainer((*val)->container,
												 JB_FARRAY,
												 &vcontained))
					return false;
			}
			else
			{
				uint32		i;

				/*
				 * If this is first container found in rhs array (at this
				 * depth), initialize temp lhs array of containers
				 */
				if (lhsConts == NULL)
				{
					uint32		j = 0;

					/* Make room for all possible values */
					lhsConts = palloc(sizeof(JsonbcValue) * nLhsElems);

					for (i = 0; i < nLhsElems; i++)
					{
						/* Store all lhs elements in temp array */
						rcont = JsonbcIteratorNext(val, &vval, true);
						Assert(rcont == WJB_ELEM);

						if (vval.type == jbvBinary)
							lhsConts[j++] = vval;
					}

					/* No container elements in temp array, so give up now */
					if (j == 0)
						return false;

					/* We may have only partially filled array */
					nLhsElems = j;
				}

				/* XXX: Nested array containment is O(N^2) */
				for (i = 0; i < nLhsElems; i++)
				{
					/* Nested container value (object or array) */
					JsonbcIterator *nestval,
							   *nestContained;
					bool		contains;

					nestval = JsonbcIteratorInit(lhsConts[i].val.binary.data);
					nestContained = JsonbcIteratorInit(vcontained.val.binary.data);

					contains = JsonbcDeepContains(&nestval, &nestContained);

					if (nestval)
						pfree(nestval);
					if (nestContained)
						pfree(nestContained);
					if (contains)
						break;
				}

				/*
				 * Report rhs container value is not contained if couldn't
				 * match rhs container to *some* lhs cont
				 */
				if (i == nLhsElems)
					return false;
			}
		}
	}
	else
	{
		elog(ERROR, "invalid jsonbc container type");
	}

	elog(ERROR, "unexpectedly fell off end of jsonbc container");
	return false;
}

/*
 * Hash a JsonbcValue scalar value, mixing the hash value into an existing
 * hash provided by the caller.
 *
 * Some callers may wish to independently XOR in JB_FOBJECT and JB_FARRAY
 * flags.
 */
void
JsonbcHashScalarValue(const JsonbcValue *scalarVal, uint32 *hash)
{
	uint32		tmp;

	/* Compute hash value for scalarVal */
	switch (scalarVal->type)
	{
		case jbvNull:
			tmp = 0x01;
			break;
		case jbvString:
			tmp = DatumGetUInt32(hash_any((const unsigned char *) scalarVal->val.string.val,
										  scalarVal->val.string.len));
			break;
		case jbvNumeric:
			/* Must hash equal numerics to equal hash codes */
			tmp = DatumGetUInt32(DirectFunctionCall1(hash_numeric,
								   NumericGetDatum(scalarVal->val.numeric)));
			break;
		case jbvBool:
			tmp = scalarVal->val.boolean ? 0x02 : 0x04;
			break;
		default:
			elog(ERROR, "invalid jsonbc scalar type");
			tmp = 0;			/* keep compiler quiet */
			break;
	}

	/*
	 * Combine hash values of successive keys, values and elements by rotating
	 * the previous value left 1 bit, then XOR'ing in the new
	 * key/value/element's hash value.
	 */
	*hash = (*hash << 1) | (*hash >> 31);
	*hash ^= tmp;
}

/*
 * Are two scalar JsonbcValues of the same type a and b equal?
 */
static bool
equalsJsonbcScalarValue(JsonbcValue *aScalar, JsonbcValue *bScalar)
{
	if (aScalar->type == bScalar->type)
	{
		switch (aScalar->type)
		{
			case jbvNull:
				return true;
			case jbvString:
				return lengthCompareJsonbcStringValue(aScalar, bScalar) == 0;
			case jbvNumeric:
				return DatumGetBool(DirectFunctionCall2(numeric_eq,
									   PointerGetDatum(aScalar->val.numeric),
									 PointerGetDatum(bScalar->val.numeric)));
			case jbvBool:
				return aScalar->val.boolean == bScalar->val.boolean;

			default:
				elog(ERROR, "invalid jsonbc scalar type");
		}
	}
	elog(ERROR, "jsonbc scalar type mismatch");
	return -1;
}

/*
 * Compare two scalar JsonbcValues, returning -1, 0, or 1.
 *
 * Strings are compared using the default collation.  Used by B-tree
 * operators, where a lexical sort order is generally expected.
 */
static int
compareJsonbcScalarValue(JsonbcValue *aScalar, JsonbcValue *bScalar)
{
	if (aScalar->type == bScalar->type)
	{
		switch (aScalar->type)
		{
			case jbvNull:
				return 0;
			case jbvString:
				return varstr_cmp(aScalar->val.string.val,
								  aScalar->val.string.len,
								  bScalar->val.string.val,
								  bScalar->val.string.len,
								  DEFAULT_COLLATION_OID);
			case jbvNumeric:
				return DatumGetInt32(DirectFunctionCall2(numeric_cmp,
									   PointerGetDatum(aScalar->val.numeric),
									 PointerGetDatum(bScalar->val.numeric)));
			case jbvBool:
				if (aScalar->val.boolean == bScalar->val.boolean)
					return 0;
				else if (aScalar->val.boolean > bScalar->val.boolean)
					return 1;
				else
					return -1;
			default:
				elog(ERROR, "invalid jsonbc scalar type");
		}
	}
	elog(ERROR, "jsonbc scalar type mismatch");
	return -1;
}


/*
 * Functions for manipulating the resizeable buffer used by convertJsonbc and
 * its subroutines.
 */

/*
 * Reserve 'len' bytes, at the end of the buffer, enlarging it if necessary.
 * Returns the offset to the reserved area. The caller is expected to fill
 * the reserved area later with copyToBuffer().
 */
static int
reserveFromBuffer(StringInfo buffer, int len)
{
	int			offset;

	/* Make more room if needed */
	enlargeStringInfo(buffer, len);

	/* remember current offset */
	offset = buffer->len;

	/* reserve the space */
	buffer->len += len;

	/*
	 * Keep a trailing null in place, even though it's not useful for us; it
	 * seems best to preserve the invariants of StringInfos.
	 */
	buffer->data[buffer->len] = '\0';

	return offset;
}

/*
 * Copy 'len' bytes to a previously reserved area in buffer.
 */
static void
copyToBuffer(StringInfo buffer, int offset, const char *data, int len)
{
	memcpy(buffer->data + offset, data, len);
}

/*
 * A shorthand for reserveFromBuffer + copyToBuffer.
 */
static void
appendToBuffer(StringInfo buffer, const char *data, int len)
{
	int			offset;

	offset = reserveFromBuffer(buffer, len);
	copyToBuffer(buffer, offset, data, len);
}


/*
 * Append padding, so that the length of the StringInfo is int-aligned.
 * Returns the number of padding bytes appended.
 */
static short
padBufferToInt(StringInfo buffer)
{
	int			padlen,
				p,
				offset;

	padlen = INTALIGN(buffer->len) - buffer->len;

	offset = reserveFromBuffer(buffer, padlen);

	/* padlen must be small, so this is probably faster than a memset */
	for (p = 0; p < padlen; p++)
		buffer->data[offset + p] = '\0';

	return padlen;
}

/*
 * Given a JsonbcValue, convert to Jsonbc. The result is palloc'd.
 */
static Jsonbc *
convertToJsonbc(JsonbcValue *val)
{
	StringInfoData buffer;
	JEntry		jentry;
	Jsonbc	   *res;

	/* Should not already have binary representation */
	Assert(val->type != jbvBinary);

	/* Allocate an output buffer. It will be enlarged as needed */
	initStringInfo(&buffer);

	/* Make room for the varlena header */
	reserveFromBuffer(&buffer, VARHDRSZ);

	convertJsonbcValue(&buffer, &jentry, val, 0);

	/*
	 * Note: the JEntry of the root is discarded. Therefore the root
	 * JsonbcContainer struct must contain enough information to tell what kind
	 * of value it is.
	 */

	res = (Jsonbc *) buffer.data;

	SET_VARSIZE(res, buffer.len);

	return res;
}

/*
 * Subroutine of convertJsonbc: serialize a single JsonbcValue into buffer.
 *
 * The JEntry header for this node is returned in *header.  It is filled in
 * with the length of this value and appropriate type bits.  If we wish to
 * store an end offset rather than a length, it is the caller's responsibility
 * to adjust for that.
 *
 * If the value is an array or an object, this recurses. 'level' is only used
 * for debugging purposes.
 */
static void
convertJsonbcValue(StringInfo buffer, JEntry *header, JsonbcValue *val, int level)
{
	check_stack_depth();

	if (!val)
		return;

	/*
	 * A JsonbcValue passed as val should never have a type of jbvBinary, and
	 * neither should any of its sub-components. Those values will be produced
	 * by convertJsonbcArray and convertJsonbcObject, the results of which will
	 * not be passed back to this function as an argument.
	 */

	if (IsAJsonbcScalar(val))
		convertJsonbcScalar(buffer, header, val);
	else if (val->type == jbvArray)
		convertJsonbcArray(buffer, header, val, level);
	else if (val->type == jbvObject)
		convertJsonbcObject(buffer, header, val, level);
	else
		elog(ERROR, "unknown type of jsonbc container");
}

static void
convertJsonbcArray(StringInfo buffer, JEntry *pheader, JsonbcValue *val, int level)
{
	int			base_offset;
	int			i;
	int			totallen, offsets_len;
	unsigned char *offsets, *ptr;
	JEntry		header;

	int			nElems = val->val.array.nElems;

	offsets_len = MAX_VARBYTE_SIZE * nElems;
	offsets_len += offsets_len / (JB_OFFSETS_CHUNK_SIZE - MAX_VARBYTE_SIZE + 1) * sizeof(JsonbcChunkHeader);

	offsets = (unsigned char *)palloc(offsets_len);

	/* Remember where in the buffer this array starts. */
	base_offset = buffer->len;

	/* Reserve space for the JEntries of the elements. */
	ptr = offsets;

	totallen = 0;
	for (i = 0; i < nElems; i++)
	{
		JsonbcValue *elem = &val->val.array.elems[i];
		int			len;
		JEntry		meta;

		/*
		 * Convert element, producing a JEntry and appending its
		 * variable-length data to buffer
		 */
		convertJsonbcValue(buffer, &meta, elem, level + 1);

		len = JBE_OFFLENFLD(meta);
		totallen += len;

		/*
		 * Bail out if total variable-length data exceeds what will fit in a
		 * JEntry length field.  We check this in each iteration, not just
		 * once at the end, to forestall possible integer overflow.
		 */
		if (totallen > JENTRY_OFFLENMASK)
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("total size of jsonbc array elements exceeds the maximum of %u bytes",
							JENTRY_OFFLENMASK)));

		encode_varbyte(meta, &ptr);
	}

	offsets_len = ptr - offsets;
	header = (offsets_len << JB_CSHIFT);

	if (val->val.array.rawScalar)
	{
		Assert(nElems == 1);
		Assert(level == 0);
		header |= JB_FSCALAR;
	}
	else
	{
		header |= JB_FARRAY;
	}

	offsets_len += varbyte_size(header);

	reserveFromBuffer(buffer, offsets_len);
	memmove(buffer->data + base_offset + offsets_len, buffer->data + base_offset, buffer->len - base_offset - offsets_len);

	ptr = (unsigned char *)buffer->data + base_offset;
	encode_varbyte(header, &ptr);
	memcpy(ptr, offsets, offsets_len - varbyte_size(header));

	/* Total data size is everything we've appended to buffer */
	totallen = buffer->len - base_offset;

	/* Check length again, since we didn't include the metadata above */
	if (totallen > JENTRY_OFFLENMASK)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("total size of jsonbc array elements exceeds the maximum of %u bytes",
						JENTRY_OFFLENMASK)));

	/* Initialize the header of this node in the container's JEntry array */
	*pheader = JENTRY_ISCONTAINER | (totallen << JENTRY_SHIFT);
}

static void
convertJsonbcObject(StringInfo buffer, JEntry *pheader, JsonbcValue *val, int level)
{
	int			base_offset;
	int			i;
	int			totallen, offsets_len;
	unsigned char *offsets, *ptr;
	JEntry		header;
	int			nPairs = val->val.object.nPairs;
	uint32		prev_key;

	/* Remember where in the buffer this object starts. */
	base_offset = buffer->len;

	offsets = (unsigned char *) palloc(MAX_VARBYTE_SIZE * nPairs * 2);
	ptr = offsets;

	/*
	 * Iterate over the keys, then over the values, since that is the ordering
	 * we want in the on-disk representation.
	 */
	totallen = 0;
	prev_key = 0;
	for (i = 0; i < nPairs; i++)
	{
		JsonbcPair  *pair = &val->val.object.pairs[i];
		int			len;
		JEntry		meta;

		/*
		 * Convert value, producing a JEntry and appending its variable-length
		 * data to buffer
		 */
		convertJsonbcValue(buffer, &meta, &pair->value, level + 1);

		len = JBE_OFFLENFLD(meta);
		totallen += len;

		/*
		 * Bail out if total variable-length data exceeds what will fit in a
		 * JEntry length field.  We check this in each iteration, not just
		 * once at the end, to forestall possible integer overflow.
		 */
		if (totallen > JENTRY_OFFLENMASK)
			ereport(ERROR,
					(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
					 errmsg("total size of jsonbc object elements exceeds the maximum of %u bytes",
							JENTRY_OFFLENMASK)));

		Assert(pair->key > prev_key);
		encode_varbyte(pair->key - prev_key, &ptr);
		encode_varbyte(meta, &ptr);

		prev_key = pair->key;
	}

	offsets_len = ptr - offsets;
	header = (offsets_len << JB_CSHIFT) | JB_FOBJECT;
	offsets_len += varbyte_size(header);

	reserveFromBuffer(buffer, offsets_len);
	memmove(buffer->data + base_offset + offsets_len, buffer->data + base_offset,
			buffer->len - base_offset - offsets_len);

	ptr = (unsigned char *)buffer->data + base_offset;
	encode_varbyte(header, &ptr);
	memcpy(ptr, offsets, offsets_len - varbyte_size(header));

	/* Total data size is everything we've appended to buffer */
	totallen = buffer->len - base_offset;

	/* Check length again, since we didn't include the metadata above */
	if (totallen > JENTRY_OFFLENMASK)
		ereport(ERROR,
				(errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
				 errmsg("total size of jsonbc object elements exceeds the maximum of %u bytes",
						JENTRY_OFFLENMASK)));

	/* Initialize the header of this node in the container's JEntry array */
	*pheader = JENTRY_ISCONTAINER | (totallen << JENTRY_SHIFT);
}

static void
convertJsonbcScalar(StringInfo buffer, JEntry *jentry, JsonbcValue *scalarVal)
{
	int			numlen;
	short		padlen = 0;
	uint32		small;

	switch (scalarVal->type)
	{
		case jbvNull:
			*jentry = JENTRY_ISNULL;
			break;

		case jbvString:
			appendToBuffer(buffer, scalarVal->val.string.val, scalarVal->val.string.len);

			*jentry = JENTRY_ISSTRING | (scalarVal->val.string.len << JENTRY_SHIFT);
			break;

		case jbvNumeric:
			if (numeric_get_small(scalarVal->val.numeric, &small))
			{
				int size = varbyte_size(small);
				unsigned char *ptr;

				reserveFromBuffer(buffer, size);
				ptr = (unsigned char *)buffer->data + buffer->len - size;
				encode_varbyte(small, &ptr);

				*jentry = JENTRY_ISINTEGER | (size << JENTRY_SHIFT);
			}
			else
			{
				numlen = VARSIZE_ANY(scalarVal->val.numeric);
				/*padlen = padBufferToInt(buffer);*/

				appendToBuffer(buffer, (char *) scalarVal->val.numeric, numlen);

				*jentry = JENTRY_ISNUMERIC | ((padlen + numlen) << JENTRY_SHIFT);
			}
			break;

		case jbvBool:
			*jentry = (scalarVal->val.boolean) ?
				JENTRY_ISBOOL_TRUE : JENTRY_ISBOOL_FALSE;
			break;

		default:
			elog(ERROR, "invalid jsonbc scalar type");
	}
}

/*
 * Compare two jbvString JsonbcValue values, a and b.
 *
 * This is a special qsort() comparator used to sort strings in certain
 * internal contexts where it is sufficient to have a well-defined sort order.
 * In particular, object pair keys are sorted according to this criteria to
 * facilitate cheap binary searches where we don't care about lexical sort
 * order.
 *
 * a and b are first sorted based on their length.  If a tie-breaker is
 * required, only then do we consider string binary equality.
 */
static int
lengthCompareJsonbcStringValue(const void *a, const void *b)
{
	const JsonbcValue *va = (const JsonbcValue *) a;
	const JsonbcValue *vb = (const JsonbcValue *) b;
	int			res;

	Assert(va->type == jbvString);
	Assert(vb->type == jbvString);

	if (va->val.string.len == vb->val.string.len)
	{
		res = memcmp(va->val.string.val, vb->val.string.val, va->val.string.len);
	}
	else
	{
		res = (va->val.string.len > vb->val.string.len) ? 1 : -1;
	}

	return res;
}

/*
 * qsort_arg() comparator to compare JsonbcPair values.
 *
 * Third argument 'binequal' may point to a bool. If it's set, *binequal is set
 * to true iff a and b have full binary equality, since some callers have an
 * interest in whether the two values are equal or merely equivalent.
 *
 * N.B: String comparisons here are "length-wise"
 *
 * Pairs with equals keys are ordered such that the order field is respected.
 */
static int
compareJsonbcPair(const void *a, const void *b, void *binequal)
{
	const JsonbcPair *pa = (const JsonbcPair *) a;
	const JsonbcPair *pb = (const JsonbcPair *) b;

	if (pa->key != pb->key)
		return (pa->key < pb->key) ? -1 : 1;

	if (binequal)
		*((bool *) binequal) = true;

	return (pa->order > pb->order) ? -1 : 1;
}

/*
 * Sort and unique-ify pairs in JsonbcValue object
 */
static void
uniqueifyJsonbcObject(JsonbcValue *object)
{
	bool		hasNonUniq = false;

	Assert(object->type == jbvObject);

	if (object->val.object.nPairs > 1)
		qsort_arg(object->val.object.pairs, object->val.object.nPairs, sizeof(JsonbcPair),
				compareJsonbcPair, &hasNonUniq);

	if (hasNonUniq)
	{
		JsonbcPair  *ptr = object->val.object.pairs + 1,
				   *res = object->val.object.pairs;

		while (ptr - object->val.object.pairs < object->val.object.nPairs)
		{
			/* Avoid copying over duplicate */
			if (lengthCompareJsonbcStringValue(ptr, res) != 0)
			{
				res++;
				if (ptr != res)
					memcpy(res, ptr, sizeof(JsonbcPair));
			}
			ptr++;
		}

		object->val.object.nPairs = res + 1 - object->val.object.pairs;
	}
}
