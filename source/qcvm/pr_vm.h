#ifndef PR_VM_H
#define PR_VM_H

#include <stdint.h>

static inline int PR_VM_TruthyBits (uint32_t bits)
{
	return (bits & UINT32_C(0x7fffffff)) != 0;
}

static inline int PR_VM_TruthyFloat (float value)
{
	union
	{
		float value;
		uint32_t bits;
	} raw = {value};

	return PR_VM_TruthyBits(raw.bits);
}

static inline float PR_VM_LogicalAnd (uint32_t lhs, uint32_t rhs)
{
	return PR_VM_TruthyBits(lhs) && PR_VM_TruthyBits(rhs);
}

static inline float PR_VM_LogicalOr (uint32_t lhs, uint32_t rhs)
{
	return PR_VM_TruthyBits(lhs) || PR_VM_TruthyBits(rhs);
}

static inline float PR_VM_LogicalNot (uint32_t value)
{
	return !PR_VM_TruthyBits(value);
}

static inline void PR_VM_StoreScalar (const eval_t *source, eval_t *target)
{
	target->_int = source->_int;
}

static inline void PR_VM_StoreVector (const eval_t *source, eval_t *target)
{
	target->vector[0] = source->vector[0];
	target->vector[1] = source->vector[1];
	target->vector[2] = source->vector[2];
}

static inline void PR_VM_AddStorePFloat (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->_float = target->_float += value->_float;
}

static inline void PR_VM_SubStorePFloat (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->_float = target->_float -= value->_float;
}

static inline void PR_VM_MulStorePFloat (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->_float = target->_float *= value->_float;
}

static inline void PR_VM_DivStorePFloat (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->_float = target->_float /= value->_float;
}

static inline void PR_VM_AddStorePVector (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->vector[0] = target->vector[0] += value->vector[0];
	result->vector[1] = target->vector[1] += value->vector[1];
	result->vector[2] = target->vector[2] += value->vector[2];
}

static inline void PR_VM_SubStorePVector (const eval_t *value, eval_t *target,
	eval_t *result)
{
	result->vector[0] = target->vector[0] -= value->vector[0];
	result->vector[1] = target->vector[1] -= value->vector[1];
	result->vector[2] = target->vector[2] -= value->vector[2];
}

static inline void PR_VM_MulStorePVectorFloat (const eval_t *value,
	eval_t *target, eval_t *result)
{
	float scale = value->_float;

	result->vector[0] = target->vector[0] *= scale;
	result->vector[1] = target->vector[1] *= scale;
	result->vector[2] = target->vector[2] *= scale;
}

static inline void PR_VM_BitSetStorePFloat (const eval_t *value,
	eval_t *target)
{
	target->_float = (int)target->_float | (int)value->_float;
}

static inline void PR_VM_BitClearStorePFloat (const eval_t *value,
	eval_t *target)
{
	target->_float = (int)target->_float & ~(int)value->_float;
}

static inline float PR_VM_RandomUnit (int random_value)
{
	return (random_value & 0x7fff) / 32768.0f;
}

static inline float PR_VM_RandomScale (int random_value, float scale)
{
	return PR_VM_RandomUnit(random_value) * scale;
}

static inline float PR_VM_RandomRange (int random_value, float minimum,
	float maximum)
{
	return minimum + PR_VM_RandomUnit(random_value) * (maximum - minimum);
}

#define PR_VM_GLOBAL_POINTER_TAG UINT32_C(0x80000000)

static inline int PR_VM_GlobalPointer (uint32_t byte_offset)
{
	return (int)(PR_VM_GLOBAL_POINTER_TAG | byte_offset);
}

static inline qboolean PR_VM_IsGlobalPointer (int pointer)
{
	return ((uint32_t)pointer & PR_VM_GLOBAL_POINTER_TAG) != 0;
}

static inline uint32_t PR_VM_GlobalPointerOffset (int pointer)
{
	return (uint32_t)pointer & ~PR_VM_GLOBAL_POINTER_TAG;
}

static inline int PR_VM_AddPointerWords (int pointer, int words)
{
	if (PR_VM_IsGlobalPointer(pointer))
		return PR_VM_GlobalPointer(PR_VM_GlobalPointerOffset(pointer) + words * 4);
	return pointer + words * 4;
}

static inline eval_t *PR_VM_PointerAddress (int pointer, void *globals,
	void *entities)
{
	if (PR_VM_IsGlobalPointer(pointer))
		return (eval_t *)((byte *)globals + PR_VM_GlobalPointerOffset(pointer));
	return (eval_t *)((byte *)entities + pointer);
}

static inline qboolean PR_VM_InBounds (int value, int lower, int upper)
{
	return (uint32_t)value >= (uint32_t)lower
		&& (uint32_t)value < (uint32_t)upper;
}

#define PR_VM_BINARY_HANDLER(name, expression) \
	static inline void name (const eval_t *a, const eval_t *b, eval_t *c) \
	{ c->_float = (expression); }

PR_VM_BINARY_HANDLER(PR_VM_AddF, a->_float + b->_float)
PR_VM_BINARY_HANDLER(PR_VM_SubF, a->_float - b->_float)
PR_VM_BINARY_HANDLER(PR_VM_MulF, a->_float * b->_float)
PR_VM_BINARY_HANDLER(PR_VM_DivF, a->_float / b->_float)
PR_VM_BINARY_HANDLER(PR_VM_BitAnd, (int)a->_float & (int)b->_float)
PR_VM_BINARY_HANDLER(PR_VM_BitOr, (int)a->_float | (int)b->_float)
PR_VM_BINARY_HANDLER(PR_VM_GreaterEqual, a->_float >= b->_float)
PR_VM_BINARY_HANDLER(PR_VM_LessEqual, a->_float <= b->_float)
PR_VM_BINARY_HANDLER(PR_VM_Greater, a->_float > b->_float)
PR_VM_BINARY_HANDLER(PR_VM_Less, a->_float < b->_float)
PR_VM_BINARY_HANDLER(PR_VM_EqualF, a->_float == b->_float)
PR_VM_BINARY_HANDLER(PR_VM_NotEqualF, a->_float != b->_float)
PR_VM_BINARY_HANDLER(PR_VM_EqualEntity, a->_int == b->_int)
PR_VM_BINARY_HANDLER(PR_VM_NotEqualEntity, a->_int != b->_int)
PR_VM_BINARY_HANDLER(PR_VM_EqualFunction, a->function == b->function)
PR_VM_BINARY_HANDLER(PR_VM_NotEqualFunction, a->function != b->function)
PR_VM_BINARY_HANDLER(PR_VM_And,
	PR_VM_LogicalAnd((uint32_t)a->_int, (uint32_t)b->_int))
PR_VM_BINARY_HANDLER(PR_VM_Or,
	PR_VM_LogicalOr((uint32_t)a->_int, (uint32_t)b->_int))

#undef PR_VM_BINARY_HANDLER

static inline void PR_VM_AddV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->vector[0] = a->vector[0] + b->vector[0];
	c->vector[1] = a->vector[1] + b->vector[1];
	c->vector[2] = a->vector[2] + b->vector[2];
}

static inline void PR_VM_SubV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->vector[0] = a->vector[0] - b->vector[0];
	c->vector[1] = a->vector[1] - b->vector[1];
	c->vector[2] = a->vector[2] - b->vector[2];
}

static inline void PR_VM_DotV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->_float = a->vector[0] * b->vector[0]
		+ a->vector[1] * b->vector[1]
		+ a->vector[2] * b->vector[2];
}

static inline void PR_VM_MulFV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->vector[0] = a->_float * b->vector[0];
	c->vector[1] = a->_float * b->vector[1];
	c->vector[2] = a->_float * b->vector[2];
}

static inline void PR_VM_MulVF (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->vector[0] = b->_float * a->vector[0];
	c->vector[1] = b->_float * a->vector[1];
	c->vector[2] = b->_float * a->vector[2];
}

static inline void PR_VM_NotF (const eval_t *a, const eval_t *b, eval_t *c)
{
	(void)b;
	c->_float = PR_VM_LogicalNot((uint32_t)a->_int);
}

static inline void PR_VM_NotV (const eval_t *a, const eval_t *b, eval_t *c)
{
	(void)b;
	c->_float = !PR_VM_TruthyFloat(a->vector[0])
		&& !PR_VM_TruthyFloat(a->vector[1])
		&& !PR_VM_TruthyFloat(a->vector[2]);
}

static inline void PR_VM_NotFunction (const eval_t *a, const eval_t *b, eval_t *c)
{
	(void)b;
	c->_float = !a->function;
}

static inline void PR_VM_ConvertFloatToInt (const eval_t *a, const eval_t *b,
	eval_t *c)
{
	(void)b;
	c->_int = (int)a->_float;
}

static inline void PR_VM_ConvertIntToFloat (const eval_t *a, const eval_t *b,
	eval_t *c)
{
	(void)b;
	c->_float = (float)a->_int;
}

static inline void PR_VM_AddInt (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->_int = (int)((uint32_t)a->_int + (uint32_t)b->_int);
}

static inline void PR_VM_MulInt (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->_int = (int)((uint32_t)a->_int * (uint32_t)b->_int);
}

static inline void PR_VM_EqualV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->_float = a->vector[0] == b->vector[0]
		&& a->vector[1] == b->vector[1]
		&& a->vector[2] == b->vector[2];
}

static inline void PR_VM_NotEqualV (const eval_t *a, const eval_t *b, eval_t *c)
{
	c->_float = a->vector[0] != b->vector[0]
		|| a->vector[1] != b->vector[1]
		|| a->vector[2] != b->vector[2];
}

#define PR_VM_CORE_OPCODES(X) \
	X(OP_ADD_F, PR_VM_AddF) \
	X(OP_ADD_V, PR_VM_AddV) \
	X(OP_SUB_F, PR_VM_SubF) \
	X(OP_SUB_V, PR_VM_SubV) \
	X(OP_MUL_F, PR_VM_MulF) \
	X(OP_MUL_V, PR_VM_DotV) \
	X(OP_MUL_FV, PR_VM_MulFV) \
	X(OP_MUL_VF, PR_VM_MulVF) \
	X(OP_DIV_F, PR_VM_DivF) \
	X(OP_BITAND, PR_VM_BitAnd) \
	X(OP_BITOR, PR_VM_BitOr) \
	X(OP_GE, PR_VM_GreaterEqual) \
	X(OP_LE, PR_VM_LessEqual) \
	X(OP_GT, PR_VM_Greater) \
	X(OP_LT, PR_VM_Less) \
	X(OP_AND, PR_VM_And) \
	X(OP_OR, PR_VM_Or) \
	X(OP_NOT_F, PR_VM_NotF) \
	X(OP_NOT_V, PR_VM_NotV) \
	X(OP_NOT_FNC, PR_VM_NotFunction) \
	X(OP_EQ_F, PR_VM_EqualF) \
	X(OP_EQ_V, PR_VM_EqualV) \
	X(OP_EQ_E, PR_VM_EqualEntity) \
	X(OP_EQ_FNC, PR_VM_EqualFunction) \
	X(OP_NE_F, PR_VM_NotEqualF) \
	X(OP_NE_V, PR_VM_NotEqualV) \
	X(OP_NE_E, PR_VM_NotEqualEntity) \
	X(OP_NE_FNC, PR_VM_NotEqualFunction) \
	X(OP_ADD_I, PR_VM_AddInt) \
	X(OP_CONV_ITOF, PR_VM_ConvertIntToFloat) \
	X(OP_CONV_FTOI, PR_VM_ConvertFloatToInt) \
	X(OP_MUL_I, PR_VM_MulInt)


static inline qboolean PR_VM_ExecuteCoreOpcode (int opcode,
	const eval_t *a, const eval_t *b, eval_t *c)
{
	switch (opcode)
	{
#define PR_VM_TEST_CASE(op, handler) case op: handler(a, b, c); break;
	PR_VM_CORE_OPCODES(PR_VM_TEST_CASE)
#undef PR_VM_TEST_CASE
	default: return false;
	}
	return true;
}

#endif
