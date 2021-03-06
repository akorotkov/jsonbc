/*-------------------------------------------------------------------------
 *
 * jsonbc.h
 *	  Declarations for jsonbc data type support.
 *
 * Copyright (c) 1996-2014, PostgreSQL Global Development Group
 *
 * src/include/utils/jsonbc.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef __JSONB_H__
#define __JSONB_H__

#include "lib/stringinfo.h"
#include "utils/array.h"
#include "utils/numeric.h"

/* Tokens used when sequentially processing a jsonbc value */
typedef enum
{
	WJB_DONE,
	WJB_KEY,
	WJB_VALUE,
	WJB_ELEM,
	WJB_BEGIN_ARRAY,
	WJB_END_ARRAY,
	WJB_BEGIN_OBJECT,
	WJB_END_OBJECT
} JsonbcIteratorToken;

/* Strategy numbers for GIN index opclasses */
#define JsonbcContainsStrategyNumber		7
#define JsonbcExistsStrategyNumber		9
#define JsonbcExistsAnyStrategyNumber	10
#define JsonbcExistsAllStrategyNumber	11

/*
 * In the standard jsonbc_ops GIN opclass for jsonbc, we choose to index both
 * keys and values.  The storage format is text.  The first byte of the text
 * string distinguishes whether this is a key (always a string), null value,
 * boolean value, numeric value, or string value.  However, array elements
 * that are strings are marked as though they were keys; this imprecision
 * supports the definition of the "exists" operator, which treats array
 * elements like keys.  The remainder of the text string is empty for a null
 * value, "t" or "f" for a boolean value, a normalized print representation of
 * a numeric value, or the text of a string value.  However, if the length of
 * this text representation would exceed JGIN_MAXLENGTH bytes, we instead hash
 * the text representation and store an 8-hex-digit representation of the
 * uint32 hash value, marking the prefix byte with an additional bit to
 * distinguish that this has happened.  Hashing long strings saves space and
 * ensures that we won't overrun the maximum entry length for a GIN index.
 * (But JGIN_MAXLENGTH is quite a bit shorter than GIN's limit.  It's chosen
 * to ensure that the on-disk text datum will have a short varlena header.)
 * Note that when any hashed item appears in a query, we must recheck index
 * matches against the heap tuple; currently, this costs nothing because we
 * must always recheck for other reasons.
 */
#define JGINFLAG_KEY	0x01	/* key (or string array element) */
#define JGINFLAG_NULL	0x02	/* null value */
#define JGINFLAG_BOOL	0x03	/* boolean value */
#define JGINFLAG_NUM	0x04	/* numeric value */
#define JGINFLAG_STR	0x05	/* string value (if not an array element) */
#define JGINFLAG_HASHED 0x10	/* OR'd into flag if value was hashed */
#define JGIN_MAXLENGTH	125		/* max length of text part before hashing */

/* Convenience macros */
#define DatumGetJsonbc(d)	((Jsonbc *) PG_DETOAST_DATUM(d))
#define JsonbcGetDatum(p)	PointerGetDatum(p)
#define PG_GETARG_JSONB(x)	DatumGetJsonbc(PG_GETARG_DATUM(x))
#define PG_RETURN_JSONB(x)	PG_RETURN_POINTER(x)

typedef struct JsonbcPair JsonbcPair;
typedef struct JsonbcValue JsonbcValue;

/*
 * Jsonbcs are varlena objects, so must meet the varlena convention that the
 * first int32 of the object contains the total object size in bytes.  Be sure
 * to use VARSIZE() and SET_VARSIZE() to access it, though!
 *
 * Jsonbc is the on-disk representation, in contrast to the in-memory JsonbcValue
 * representation.  Often, JsonbcValues are just shims through which a Jsonbc
 * buffer is accessed, but they can also be deep copied and passed around.
 *
 * Jsonbc is a tree structure. Each node in the tree consists of a JEntry
 * header and a variable-length content (possibly of zero size).  The JEntry
 * header indicates what kind of a node it is, e.g. a string or an array,
 * and provides the length of its variable-length portion.
 *
 * The JEntry and the content of a node are not stored physically together.
 * Instead, the container array or object has an array that holds the JEntrys
 * of all the child nodes, followed by their variable-length portions.
 *
 * The root node is an exception; it has no parent array or object that could
 * hold its JEntry. Hence, no JEntry header is stored for the root node.  It
 * is implicitly known that the root node must be an array or an object,
 * so we can get away without the type indicator as long as we can distinguish
 * the two.  For that purpose, both an array and an object begin with a uint32
 * header field, which contains an JB_FOBJECT or JB_FARRAY flag.  When a naked
 * scalar value needs to be stored as a Jsonbc value, what we actually store is
 * an array with one element, with the flags in the array's header field set
 * to JB_FSCALAR | JB_FARRAY.
 *
 * Overall, the Jsonbc struct requires 4-bytes alignment. Within the struct,
 * the variable-length portion of some node types is aligned to a 4-byte
 * boundary, while others are not. When alignment is needed, the padding is
 * in the beginning of the node that requires it. For example, if a numeric
 * node is stored after a string node, so that the numeric node begins at
 * offset 3, the variable-length portion of the numeric node will begin with
 * one padding byte so that the actual numeric data is 4-byte aligned.
 */

/*
 * JEntry format.
 *
 * The least significant 28 bits store either the data length of the entry,
 * or its end+1 offset from the start of the variable-length portion of the
 * containing object.  The next three bits store the type of the entry, and
 * the high-order bit tells whether the least significant bits store a length
 * or an offset.
 *
 * The reason for the offset-or-length complication is to compromise between
 * access speed and data compressibility.  In the initial design each JEntry
 * always stored an offset, but this resulted in JEntry arrays with horrible
 * compressibility properties, so that TOAST compression of a JSONB did not
 * work well.  Storing only lengths would greatly improve compressibility,
 * but it makes random access into large arrays expensive (O(N) not O(1)).
 * So what we do is store an offset in every JB_OFFSET_STRIDE'th JEntry and
 * a length in the rest.  This results in reasonably compressible data (as
 * long as the stride isn't too small).  We may have to examine as many as
 * JB_OFFSET_STRIDE JEntrys in order to find out the offset or length of any
 * given item, but that's still O(1) no matter how large the container is.
 *
 * We could avoid eating a flag bit for this purpose if we were to store
 * the stride in the container header, or if we were willing to treat the
 * stride as an unchangeable constant.  Neither of those options is very
 * attractive though.
 */
typedef uint32 JEntry;

#define JENTRY_SHIFT			0x3
#define JENTRY_OFFLENMASK		0x10000000

#define JENTRY_TYPEMASK			0x7

/* values stored in the type bits */
#define JENTRY_ISSTRING			0x1
#define JENTRY_ISNUMERIC		0x2
#define JENTRY_ISINTEGER		0x3
#define JENTRY_ISBOOL_FALSE		0x4
#define JENTRY_ISBOOL_TRUE		0x5
#define JENTRY_ISNULL			0x6
#define JENTRY_ISCONTAINER		0x7 /* array or object */

/* Access macros.  Note possible multiple evaluations */
#define JBE_OFFLENFLD(je_)		((je_) >> JENTRY_SHIFT)
#define JBE_HAS_OFF(je_)		(((je_) & JENTRY_HAS_OFF) != 0)
#define JBE_ISSTRING(je_)		(((je_) & JENTRY_TYPEMASK) == JENTRY_ISSTRING)
#define JBE_ISNUMERIC(je_)		(((je_) & JENTRY_TYPEMASK) == JENTRY_ISNUMERIC)
#define JBE_ISCONTAINER(je_)	(((je_) & JENTRY_TYPEMASK) == JENTRY_ISCONTAINER)
#define JBE_ISNULL(je_)			(((je_) & JENTRY_TYPEMASK) == JENTRY_ISNULL)
#define JBE_ISBOOL_TRUE(je_)	(((je_) & JENTRY_TYPEMASK) == JENTRY_ISBOOL_TRUE)
#define JBE_ISBOOL_FALSE(je_)	(((je_) & JENTRY_TYPEMASK) == JENTRY_ISBOOL_FALSE)
#define JBE_ISBOOL(je_)			(JBE_ISBOOL_TRUE(je_) || JBE_ISBOOL_FALSE(je_))
#define JBE_ISINTEGER(je_)		(((je_) & JENTRY_TYPEMASK) == JENTRY_ISINTEGER)

/* Macro for advancing an offset variable to the next JEntry */
#define JBE_ADVANCE_OFFSET(offset, je) \
	do { \
		JEntry	je_ = (je); \
		if (JBE_HAS_OFF(je_)) \
			(offset) = JBE_OFFLENFLD(je_); \
		else \
			(offset) += JBE_OFFLENFLD(je_); \
	} while(0)

/*
 * We store an offset, not a length, every JB_OFFSET_STRIDE children.
 * Caution: this macro should only be referenced when creating a JSONB
 * value.  When examining an existing value, pay attention to the HAS_OFF
 * bits instead.  This allows changes in the offset-placement heuristic
 * without breaking on-disk compatibility.
 */
#define JB_OFFSET_STRIDE		32

#define	JB_OFFSETS_CHUNK_SIZE	32

/*
 * A jsonbc array or object node, within a Jsonbc Datum.
 *
 * An array has one child for each element, stored in array order.
 *
 * An object has two children for each key/value pair.  The keys all appear
 * first, in key sort order; then the values appear, in an order matching the
 * key order.  This arrangement keeps the keys compact in memory, making a
 * search for a particular key more cache-friendly.
 */
typedef struct JsonbcContainer
{
	char	data[1];

	/* the data for each child node follows. */
} JsonbcContainer;

/* flags for the header-field in JsonbcContainer */
#define JB_CSHIFT				2
#define JB_FSCALAR				0
#define JB_FOBJECT				1
#define JB_FARRAY				2
#define JB_MASK					3

/* The top-level on-disk format for a jsonbc datum. */
typedef struct
{
	int32		vl_len_;		/* varlena header (do not touch directly!) */
	JsonbcContainer root;
} Jsonbc;

extern uint32 jsonbc_header(Jsonbc *value);

/* convenience macros for accessing the root container in a Jsonbc datum */
#define JB_ROOT_COUNT(jbp_)		( jsonbc_header(jbp_) >> JB_CSHIFT)
#define JB_ROOT_IS_SCALAR(jbp_) ( (jsonbc_header(jbp_) & JB_MASK) == JB_FSCALAR)
#define JB_ROOT_IS_OBJECT(jbp_) ( (jsonbc_header(jbp_) & JB_MASK) == JB_FOBJECT)
#define JB_ROOT_IS_ARRAY(jbp_)	( (jsonbc_header(jbp_) & JB_MASK) == JB_FARRAY)


/*
 * JsonbcValue:	In-memory representation of Jsonbc.  This is a convenient
 * deserialized representation, that can easily support using the "val"
 * union across underlying types during manipulation.  The Jsonbc on-disk
 * representation has various alignment considerations.
 */
struct JsonbcValue
{
	enum
	{
		/* Scalar types */
		jbvNull = 0x0,
		jbvString,
		jbvNumeric,
		jbvBool,
		/* Composite types */
		jbvArray = 0x10,
		jbvObject,
		/* Binary (i.e. struct Jsonbc) jbvArray/jbvObject */
		jbvBinary
	}			type;			/* Influences sort order */

	union
	{
		Numeric numeric;
		bool		boolean;
		struct
		{
			int			len;
			char	   *val;	/* Not necessarily null-terminated */
		}			string;		/* String primitive type */

		struct
		{
			int			nElems;
			JsonbcValue *elems;
			bool		rawScalar;		/* Top-level "raw scalar" array? */
		}			array;		/* Array container type */

		struct
		{
			int			nPairs; /* 1 pair, 2 elements */
			JsonbcPair  *pairs;
		}			object;		/* Associative container type */

		struct
		{
			int			len;
			JsonbcContainer *data;
		}			binary;		/* Array or object, in on-disk format */
	}			val;
};

#define IsAJsonbcScalar(jsonbcval)	((jsonbcval)->type >= jbvNull && \
									 (jsonbcval)->type <= jbvBool)

/*
 * Key/value pair within an Object.
 *
 * This struct type is only used briefly while constructing a Jsonbc; it is
 * *not* the on-disk representation.
 *
 * Pairs with duplicate keys are de-duplicated.  We store the originally
 * observed pair ordering for the purpose of removing duplicates in a
 * well-defined way (which is "last observed wins").
 */
struct JsonbcPair
{
	int32		key;
	JsonbcValue	value;			/* May be of any type */
	uint32		order;			/* Pair's index in original sequence */
};

/* Conversion state used when parsing Jsonbc from text, or for type coercion */
typedef struct JsonbcParseState
{
	JsonbcValue	contVal;
	Size		size;
	struct JsonbcParseState *next;
} JsonbcParseState;

/*
 * JsonbcIterator holds details of the type for each iteration. It also stores a
 * Jsonbc varlena buffer, which can be directly accessed in some contexts.
 */
typedef enum
{
	JBI_ARRAY_START,
	JBI_ARRAY_ELEM,
	JBI_OBJECT_START,
	JBI_OBJECT_KEY,
	JBI_OBJECT_VALUE
} JsonbcIterState;

typedef struct JsonbcIterator
{
	/* Container being iterated */
	JsonbcContainer *container;
	uint32		childrenSize;
	uint32		curKey;
	bool		isScalar;		/* Pseudo-array scalar value? */
	unsigned char	   *children;		/* JEntrys for child nodes */
	unsigned char	   *childrenPtr;
	unsigned char	   *chunkEnd;
	/* Data proper.  This points to the beginning of the variable-length data */
	char	   *dataProper;


	/* Data offset corresponding to current item */
	uint32		curDataOffset;

	/*
	 * If the container is an object, we want to return keys and values
	 * alternately; so curDataOffset points to the current key, and
	 * curValueOffset points to the current value.
	 */
	uint32		curValueOffset;

	/* Private state */
	JsonbcIterState state;

	struct JsonbcIterator *parent;
} JsonbcIterator;

/* I/O routines */
extern Datum jsonbc_in(PG_FUNCTION_ARGS);
extern Datum jsonbc_out(PG_FUNCTION_ARGS);
extern Datum jsonbc_recv(PG_FUNCTION_ARGS);
extern Datum jsonbc_send(PG_FUNCTION_ARGS);
extern Datum jsonbc_typeof(PG_FUNCTION_ARGS);

/* Indexing-related ops */
extern Datum jsonbc_exists(PG_FUNCTION_ARGS);
extern Datum jsonbc_exists_any(PG_FUNCTION_ARGS);
extern Datum jsonbc_exists_all(PG_FUNCTION_ARGS);
extern Datum jsonbc_contains(PG_FUNCTION_ARGS);
extern Datum jsonbc_contained(PG_FUNCTION_ARGS);
extern Datum jsonbc_ne(PG_FUNCTION_ARGS);
extern Datum jsonbc_lt(PG_FUNCTION_ARGS);
extern Datum jsonbc_gt(PG_FUNCTION_ARGS);
extern Datum jsonbc_le(PG_FUNCTION_ARGS);
extern Datum jsonbc_ge(PG_FUNCTION_ARGS);
extern Datum jsonbc_eq(PG_FUNCTION_ARGS);
extern Datum jsonbc_cmp(PG_FUNCTION_ARGS);
extern Datum jsonbc_hash(PG_FUNCTION_ARGS);

/* jsonfuncs.c */
extern Datum jsonbc_object_field(PG_FUNCTION_ARGS);
extern Datum jsonbc_object_field_text(PG_FUNCTION_ARGS);
extern Datum jsonbc_array_element(PG_FUNCTION_ARGS);
extern Datum jsonbc_array_element_text(PG_FUNCTION_ARGS);
extern Datum jsonbc_extract_path(PG_FUNCTION_ARGS);
extern Datum jsonbc_extract_path_text(PG_FUNCTION_ARGS);
extern Datum jsonbc_object_keys(PG_FUNCTION_ARGS);
extern Datum jsonbc_array_length(PG_FUNCTION_ARGS);
extern Datum jsonbc_each(PG_FUNCTION_ARGS);
extern Datum jsonbc_each_text(PG_FUNCTION_ARGS);
extern Datum jsonbc_array_elements_text(PG_FUNCTION_ARGS);
extern Datum jsonbc_array_elements(PG_FUNCTION_ARGS);
extern Datum jsonbc_populate_record(PG_FUNCTION_ARGS);
extern Datum jsonbc_populate_recordset(PG_FUNCTION_ARGS);
extern Datum jsonbc_to_record(PG_FUNCTION_ARGS);
extern Datum jsonbc_to_recordset(PG_FUNCTION_ARGS);

/* GIN support functions for jsonbc_ops */
extern Datum gin_compare_jsonbc(PG_FUNCTION_ARGS);
extern Datum gin_extract_jsonbc(PG_FUNCTION_ARGS);
extern Datum gin_extract_jsonbc_query(PG_FUNCTION_ARGS);
extern Datum gin_consistent_jsonbc(PG_FUNCTION_ARGS);
extern Datum gin_triconsistent_jsonbc(PG_FUNCTION_ARGS);

/* GIN support functions for jsonbc_path_ops */
extern Datum gin_extract_jsonbc_path(PG_FUNCTION_ARGS);
extern Datum gin_extract_jsonbc_query_path(PG_FUNCTION_ARGS);
extern Datum gin_consistent_jsonbc_path(PG_FUNCTION_ARGS);
extern Datum gin_triconsistent_jsonbc_path(PG_FUNCTION_ARGS);

/* Support functions */
extern uint32 getJsonbcOffset(const JsonbcContainer *jc, int index);
extern int	compareJsonbcContainers(JsonbcContainer *a, JsonbcContainer *b);
extern JsonbcValue *findJsonbcValueFromContainer(JsonbcContainer *sheader,
							uint32 flags,
							JsonbcValue *key);
extern JsonbcValue *getIthJsonbcValueFromContainer(JsonbcContainer *sheader,
							  uint32 i);
extern JsonbcValue *pushJsonbcValue(JsonbcParseState **pstate,
			   JsonbcIteratorToken seq, JsonbcValue *scalarVal);
extern JsonbcIterator *JsonbcIteratorInit(JsonbcContainer *container);
extern JsonbcIteratorToken JsonbcIteratorNext(JsonbcIterator **it, JsonbcValue *val,
				  bool skipNested);
extern Jsonbc *JsonbcValueToJsonbc(JsonbcValue *val);
extern bool JsonbcDeepContains(JsonbcIterator **val,
				  JsonbcIterator **mContained);
extern void JsonbcHashScalarValue(const JsonbcValue *scalarVal, uint32 *hash);

/* jsonbc.c support function */
extern char *JsonbcToCString(StringInfo out, JsonbcContainer *in,
			   int estimated_len);

/* numeric_utils.c support function */
extern bool numeric_get_small(Numeric value, uint32 *out);
extern Numeric small_to_numeric(uint32 value);

extern int jsonbc_root_max_count(Jsonbc *value);

#endif   /* __JSONB_H__ */
